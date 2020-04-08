# ztnc
Send files across networks with minimal configuration.

## Building
Requires a C++14 compatible compiler and Boost.

### macOS and Linux
`mkdir build && cmake -B ./build . && cmake --build ./build`

### Windows
Not supported currently. Libzt does not support MSVC (or at least has errors).  

## Usage
Client: `ztnc [-n <network id>] [-c <cache dir>] <address> <port>`  
Server: `ztnc [-n <network id>] [-c <cache dir>] -l <port>`

## Options
Network ID: By default `ztnc` will use the Earth network (8056c2e21c000001) but you may
specify a custom network if you have one.

Cache Dir: By default, `ztnc` will create a new temporary Node ID and cache its credentials
in `/tmp`. If you prefer to keep the same Node ID (and IP address) you may specify a
directory to store cached identity files.

## Known Bugs
xonsh is somehow unable to pipe entire files to `ztnc` with a construction like
`cat file | ztnc` without hanging and crashing. This appears to be a bug in xonsh as
bash does not exhibit this behavior.
