Tom Kessous - 206018749
Tomer Laor - 206783813

*Compile: cc -O0 PrimeProbe.c -o PrimeProbe
*Run: ./PrimeProbe

*The "main" function in the "PrimeProbe.c" file generates the heat-map and the associativity figures.

*The associativity figure illustrates the associativity of our CPU. It is evident that having more than 8 lines per cache set leads to a high probe time, indicating that those lines cannot fully reside within the cache set. Therefore, the associativity of our CPU is 8.

*The heat-map figure demonstrates that as we add external lines to the cache sets, the probe time increases in correlation with the number of external lines.
