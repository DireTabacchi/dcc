#!/usr/bin/env bash

printUsage() {
    echo "Usage: $0 [help] [build_type [action]]" 
    echo 'build type:'
    echo -e '\tdebug\n\trelease'
    echo 'action:'
    echo -e '\tclean\t\tRemove object files and executable from build_type directory'
    echo -e '\tnuke\t\tRemove entire build_type directory'
}

buildDebug() {
    if [[ $# -le 2 ]]; then
        if [[ $# -eq 2 ]]; then
            if [[ "$2" == 'clean' ]]; then
                if [[ ! -d debug ]]; then
                    echo 'Nothing to clean.'
                    exit 0
                fi
                echo 'Cleaning debug build directory...'
                make -f MakeDebug.mk clean
                echo 'Done.'
                exit 0
            elif [[ "$2" == 'nuke' ]]; then
                if [[ ! -d debug ]]; then
                    echo 'Nothing to nuke.'
                    exit 0
                fi
                echo 'Nuking debug build directory...'
                make -f MakeDebug.mk nuke
                echo 'Done.'
                exit 0
            else
                echo "[Error] unknown action '$2'"
                printUsage
                exit 1
            fi
        fi
    else
        echo '[Error] too many arguments'
        printUsage
        exit 1
    fi

    echo 'Building debug...'
    if [[ ! -d debug ]]; then
        echo 'First time debug setup...'
        mkdir -p debug/obj
        echo 'First time debug setup complete.'
    fi

    echo 'Compiling debug...'
    make -f MakeDebug.mk
    echo 'Done compiling debug.'
}

buildRelease() {
    if [[ $# -le 2 ]]; then
        if [[ $# -eq 2 ]]; then
            if [[ "$2" == 'clean' ]]; then
                if [[ ! -d release ]]; then
                    echo 'Nothing to clean.'
                    exit 0
                fi
                echo 'Cleaning release build directory...'
                make -f MakeRelease.mk clean
                echo 'Done.'
                exit 0
            elif [[ "$2" == 'nuke' ]]; then
                if [[ ! -d release ]]; then
                    echo 'Nothing to nuke.'
                    exit 0
                fi
                echo 'Nuking release build directory...'
                make -f MakeRelease.mk nuke
                echo 'Done.'
                exit 0
            else
                echo "[Error] unknown action '$2'"
                printUsage
                exit 1
            fi
        fi
    else
        echo '[Error] too many arguments'
        printUsage
        exit 1
    fi

    echo 'Building release...'
    if [[ ! -d release ]]; then
        echo 'First time release setup...'
        mkdir -p release/obj
        echo 'First time release setup complete.'
    fi

    echo 'Compiling release...'
    make -f MakeRelease.mk
}

if [[ $# -eq 0 ]]; then
    buildDebug
    exit 0
fi

if [[ $# -ge 1 ]]; then
    if [[ "$1" == 'help' ]]; then
        printUsage
    elif [[ "$1" == 'debug' ]]; then
        buildDebug $@
    elif [[ "$1" == 'release' ]]; then
        buildRelease $@
    else
        echo "[Error] unknown option '$1'"
        printUsage
    fi
fi
