#!/bin/bash

BUILD_DIR=build

# Function to display help message
function display_help() {
    echo "Usage: $0 [compile|clean|help]"
    echo "  compile: Creates the build/ directory and compiles the reader/writer executables"
    echo "  clean: Remove build/ directory"
    echo "  help: Display this help message"
}

# Check the number of arguments
if [ "$#" -eq 1 ]; then
    case "$1" in
        "compile")
            # Your compile command here
            echo "Compiling..."
            mkdir -p $BUILD_DIR
            gcc -Wall -Werror -o ${BUILD_DIR}/writer writer.c
            gcc -Wall -Werror -o ${BUILD_DIR}/reader reader.c
            echo "Done!"
            ;;
        "clean")
            # Check if the directory exists
            if [ -d "${BUILD_DIR}" ]; then
                # Remove the directory
                rm -rf "${BUILD_DIR}"
                echo "Directory "${BUILD_DIR}" removed."
            fi
            ;;
        "help")
            display_help
            ;;
        *)
            echo "Invalid argument. Use 'compile', 'clean', or 'help'."
            display_help
            ;;
    esac
else
    echo "Usage: $0 [compile|clean|help]"
fi