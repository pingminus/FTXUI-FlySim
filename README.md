ftxui-flySim
-------------


![2025-11-1901-35-20-ezgif com-video-to-gif-converter](https://github.com/user-attachments/assets/d4665482-1c0c-4585-8834-ae3711591045)
<img width="1919" height="1079" alt="image" src="https://github.com/user-attachments/assets/49aa3bc4-8819-47a4-97a1-4757fc8c8316" />


# Build instructions:
Open via. VS or
~~~bash
mkdir build
cd build
cmake ..
make -j
cd ../target
./ftxui-starter
~~~
# How to Land the Plane

Landing requires putting the aircraft into a stable, low-speed, low-altitude configuration directly over the 2×2 airport zone (marked with X on the map).

## Landing Requirements

To land successfully, ALL of the following must be true at the moment altitude reaches 0:

You are directly over one of the airport tiles
(the 2×2 landing zone at (10,10) → (11,11)).

Speed below 140 kt

Gear DOWN (G)

Pitch greater than -5° (no steep nose-down impact)

Roll less than 10° (nearly wings-level)

Flaps ON (recommended but optional)
