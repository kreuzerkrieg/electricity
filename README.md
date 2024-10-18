Parse and analyze electricity consumption stats file from Israeli Electricity Company. See https://www.iec.co.il/home

To build you need decent compiler, vcpkg integration with your favorite IDE

Clone the repo

Run `cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -G Ninja -B ./build`

`cd build`

`ninja`

You are good to go

Example: `./electricity -f ./../meter-2024.csv --from 20240801T000000 --time-range 7:00-17:00`

Added rudimentary support for charts using uPlot. An HTML file with the same filename as the input file will be produced. Just open it in a browser
