# filepie [![gplv3](https://img.shields.io/aur/license/yaourt.svg)](https://www.gnu.org/licenses/gpl-3.0.html)
Terminal utility to display size quota maps on drives and folders

[![asciicast](https://asciinema.org/a/8aw5ijubuwtnp6kt6p1svr30u.png)](https://asciinema.org/a/8aw5ijubuwtnp6kt6p1svr30u)

## How to get filepie

Either checkout the package from the ppa

    sudo add-apt-repository ppa:marco-diiga/ppa
    sudo apt-get update
    sudo apt-get install filepie

or build it yourself (see *How to build* section).

## How to build

1. Install ncurses dependencies

        apt-get install ncurses-dev

2. Checkout and build

        git clone https://github.com/marcodiiga/filepie.git
        cd filepie
        cmake .
        cmake --build . --target all
        bin/filepie

## License
filepie is licensed under [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html)
