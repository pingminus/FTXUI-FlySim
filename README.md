# ftxui-flySim
-------------
# Demo NEW UI
https://github.com/user-attachments/assets/1042cd4b-0c29-4290-ab5d-dc5fcb80217c


<img width="1904" height="1026" alt="image" src="https://github.com/user-attachments/assets/18db1281-4662-43d0-b364-24b0f96a38a8" />













# OLD UI



![2025-11-1901-35-20-ezgif com-video-to-gif-converter](https://github.com/user-attachments/assets/d4665482-1c0c-4585-8834-ae3711591045)


<img width="1895" height="1043" alt="image" src="https://github.com/user-attachments/assets/2e51a2c4-a0e4-4981-bcb3-48c639ceb11b" />


# Build instructions:
Windows
~~~bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
.\Debug\ftxui-starter.exe
~~~
MacOs/Linux
~~~bash
mkdir build
cd build
cmake ..
make -j
./ftxui-starter
~~~
# How to Land the Plane

Landing requires putting the aircraft into a stable, low-speed, low-altitude configuration directly over the 2×2 airport zone (marked with pink-Box on the map).

## Landing Requirements

To land successfully, ALL of the following must be true at the moment altitude reaches 0:

You are directly over one of the airport tiles
(the 2×2 landing zone at (10,10) → (11,11)).

Speed below 140 kt

Gear DOWN (G)

Pitch greater than -5° (no steep nose-down impact)

Roll less than 10° (nearly wings-level)

Flaps ON (recommended but optional)
