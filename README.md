# memscan
a memory scanner I wrote from scratch to start learning reverse engineering
## why I built this
i used to use cheats and download them online but my curiosity got the most of me and i wanted to know how they actually work and i started with this memscanner/trainer 
## what it does
- attaches to a running process
- scans its memory for a value (known OR unknown) and if it is unknown it is kinda struggle but fun 
- narrows candidates by behavior (changed / increased / decreased)
- writes and freezes values
## how the technique works
for example i tested it on NFSmostwanted 2012 (offline) i did try two things first is to find the nitrous address to freeze it and in which i did first i did the snapshot and then stayed near a gas station in the game and kept on using the nitrous and in the scanner i kept on doing increased and decreased scans until i was down to 4 addresses and then i froze them all and then i tested each 4 of them until i reached the address that is responsible for the nitrous and froze it and i got infinite nitrous  
## build
navigate to the folder you stored the files in in the w64devkit terminal and run everything as administrator 
in the terminal navigate to the folder where the of the scanner are (cd your path)  files and then run : 
g++ -O2 -std=c++17 -o memscan.exe main.cpp

after the .exe is in the folder RUN IT AS ADMINISTRATOR and the tool will be working fine 
ONLY-USE-ON-OFFLINE-GAMES-SINGLE-PLAYER
## note
----------------------------------------
ONLY-USE-ON-OFFLINE-GAMES-SINGLE-PLAYER
----------------------------------------
- for educational use on single-player, offline games you own.
  built to learn RE, not to cheat online games BECAUSE you will get slapped with ban my friends 🥀
----------------------------------------
ONLY-USE-ON-OFFLINE-GAMES-SINGLE-PLAYER
---------------------------------------
  
```     
                                       \        / 
                                        \      /
                                         \ __ /
                                          /--\ 
                                            .
                                      \___________/
                                            |
                                            |
                                            |
                                        ____|___
                                       /________\ 
                                       |  #mem  |
                                       |  #scan |
                                       |#trainer|
                                       | #learn |
                                       |________|
```
thank you , and i hope it would help you learn something new
