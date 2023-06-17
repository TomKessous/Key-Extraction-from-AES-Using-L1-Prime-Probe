import sys
from functools import partial
from itertools import product

import numpy as np
from tqdm.contrib.concurrent import process_map


Sbox_inv = (
        0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
        0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
        0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
        0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
        0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
        0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
        0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
        0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
        0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
        0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
        0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89, 0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
        0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
        0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
        0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
        0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
        )


def load_output_file(filename):
    data = []
    with open(filename, 'r') as file:
        for line in file:
            line_data = line.strip().split()
            plaintext = line_data[0]
            ciphertext = line_data[1]
            probe_times = list(map(int, line_data[2:]))
            data.append((plaintext, ciphertext, probe_times))
    return data


def get_correlation_of_of_guess(guess, output_data, target_byte):
    byte_guess, table_guess = guess
    
    probabilities_all = []
    probe_times_all = []
    for plaintext, ciphertext, probe_times in output_data:
        
        cipher_byte = int(ciphertext[target_byte * 2: target_byte * 2 + 2], 16)
        index_guess = Sbox_inv[cipher_byte ^ byte_guess]
        index_guess_left_nibble = ((index_guess >> 4) + table_guess) % 64
        
        curr_probabilities = [0] * 64
        curr_probabilities[index_guess_left_nibble] = 1
        
        probabilities_all.append(curr_probabilities)
        probe_times_all.append(probe_times)
        
    probabilities_all = np.array(probabilities_all)
    probe_times_all = np.array(probe_times_all)
    
    corr_sum = 0
    for i in range(16):
        idx = (i + table_guess) % 64
        corr_sum += abs(np.corrcoef(probabilities_all[:, idx], probe_times_all[:, idx])[0, 1])
    
    return corr_sum / 16, guess


def get_data_without_outliers(output_data, number_of_traces, outlier_threshold=100):
    data_without_outliers = []
    for plaintext, ciphertext, probe_times in output_data:
        
        if outlier_threshold is not None and any(np.array(probe_times) > outlier_threshold):
            continue
        
        data_without_outliers.append((plaintext, ciphertext, probe_times))
        
        if len(data_without_outliers) >= number_of_traces:
            break
    return data_without_outliers
            
            
def correlation_attack(output_data, target_byte, number_of_traces=10_000):
    output_data = get_data_without_outliers(output_data, number_of_traces)
    func = partial(get_correlation_of_of_guess, output_data=output_data, target_byte=target_byte)
    correlations = process_map(func, list(product(range(256), range(64))), max_workers=8, chunksize=500)
    return correlations


def main():    
    output_file = sys.argv[1]
    target_byte = int(sys.argv[2])
    
    output_data = load_output_file(output_file)
    correlations = correlation_attack(output_data, target_byte)
    sorted_guesses = sorted(correlations, key=lambda val: val[0], reverse=True)
    top_guesses = sorted_guesses[:10]
        
    print("Top 10 Guesses:")
    for correlation, guess in top_guesses:
        byte_guess, table_guess = guess
        print(f"Byte Guess: {byte_guess}, Table Guess: {table_guess}, Correlation: {correlation}")


if __name__ == "__main__":
    main()
