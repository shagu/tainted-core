# ![icon](img/logo.png) Tainted Core [![Build Status](https://travis-ci.org/shagu/tainted-core.svg?branch=master)](https://travis-ci.org/shagu/tainted-core)

Tainted Core is a piece of software that targets emulation of a World of Warcraft: The Burning Crusade game server.
This is my personal fork of [Talamortis'](https://github.com/talamortis) [OregonCore](https://github.com/talamortis/OregonCore) fork which itself was derived from
[TrinityCore](https://github.com/TrinityCore/TrinityCore) long ago.

Why [**[Tainted Core]**](https://wowwiki-archive.fandom.com/wiki/Tainted_Core)? Because you're not supposed to keep it for yourself.

## Getting started

### Prepare/Dependencies
Make sure to have all required dependencies installed. ArchLinux is used in this example.
For debian based systems use `apt` instead of `pacman` and `yay`, also replace the packages
with the corresponding ones of your distribution.

    pacman -Sy mariadb base-devel boost cmake git
    yay -S ace

    mariadb-install-db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
    systemctl --now enable mariadb
    mysql_secure_installation

You can now fetch the latest repository:

    git clone https://github.com/shagu/tainted-core source

### Database
The first command will create all empty databases and also a new user
to your mysql database (oregon:oregon). You might want to delete or
change its password later.

    mysql -u'root' -p < source/sql/create/create_mysql.sql
    mysql -u'oregon' -p'oregon' world < source/sql/base/world.sql
    mysql -u'oregon' -p'oregon' characters < source/sql/base/characters.sql
    mysql -u'oregon' -p'oregon' realmd < source/sql/base/realmd.sql

    cd source/sql/updates/characters
    for i in *; do mysql -u"oregon" -p"oregon" characters < $i; done
    cd -

    cd source/sql/updates/realmd
    for i in *.sql; do mysql -u"oregon" -p"oregon" realmd < $i; done
    cd -

    cd source/sql/updates/world
    for i in *.sql; do mysql -u"oregon" -p"oregon" world < $i; done
    cd -

### Compile
Change `make -j8` to the number of CPUs/Threads you have. The higher the number,
the more threads will be utilizied to compile the core.

    mkdir -p build; cd build
    cmake ../source -DCMAKE_INSTALL_PREFIX=install \
        -DSERVERS=1 \
        -DTOOLS=1 \
        -DSCRIPTS=1 \
        -DWITH_DOCS=1 \
        -DWITH_WARNINGS=0 \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++

    make -j12
    make install

### Game Files
The emulator needs access to several game files found in the 2.4.3 client.
Such as maps, movement paths, spells and other data. The following commands
will extract all the required data and also generate movement maps. In this example
it is assumed, the game directory is in `~/games/Burning\ Crusade/`

    cd install/bin
    cp map_extractor movements_extractor vmap_assembler vmap_extractor ~/games/Burning\ Crusade/
    cd ~/games/Burning\ Crusade/
    mkdir vmaps
    ./map_extractor
    ./vmap_extractor
    ./vmap_assembler Buildings/ vmaps/
    ./movements_extractor

This process will now take a long time (1-4 hours). Once done, copy all files to
your blobs directory (which is right beside the build and the source directory)

    cd -
    mkdir ../../blobs
    cd ../../blobs
    mv ~/games/Burning\ Crusade/{dbc,maps,vmaps,mmaps} .
    rm ~/games/Burning\ Crusade/{map_extractor,movements_extractor,vmap_assembler,vmap_extractor}

### Starting
To launch the server, it is required to have all game-files (blobs) in the right
directory and also to have the configuration files set up properly.

    cd install
    cp etc/oregoncore.conf.dist etc/oregoncore.conf
    cp etc/oregonrealm.conf.dist etc/oregonrealm.conf
    vim etc/oregoncore.conf etc/oregonrealm.conf

    for obj in ../../blobs/*; do ln -s $obj .; done

    ./bin/oregon-realm &
    ./bin/oregon-core

In the `oregon-core` console, you can now typ the following to create a new GM account (foo:bar):

    account create foo bar
    account set gmlevel foo 3

Hit `<ctrl-c>` to stop the core. Type `fg` and hit `<ctrl+c>` again to stop the realm.
