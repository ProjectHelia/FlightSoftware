# Project HELIA: Flight Software

This repository contains the flight software for Project HELIA: source code for all MCUs, all units tests, all shared libraries, and doxygen for documentation generation.

## Code Organisation

Since the repository has gotten pretty big, I'm going to make some notes on where everything is so other people can easily navigate around.
The computer system is 6 ESP32 S3s with a CAN Bus for central communication, so although this is a single codebase it actually feels more like 6 mini code bases in one.

### `libs/`

The `libs/` folder is for shared behaviour. Basically anything that all the ESP32s need to do is placed in here.
At the start this was mostly the `can` and `fsm` packages, and now slowly as I'm noticing code duplication I'm abstracting the behaviour and putting it in a shared library within `libs`.

In the previous codebase, the shared behaviour folder got pretty big and messy, mostly because I started putting actual implementation in there rather than just abstract libraries that each ESP32 could use.
There should be a pretty hard line between what should be in the `src` folder and `libs` folder: 

- `src` is for implementation code, anything that will actually run on the ESP32s whether that is logic or hardware specific code
- `libs` is for abstract, shared libraries that code within `src` uses to improve the implementation and reduce code duplication across the codebase

A general rule of thumb, if you're starting FreeRTOS tasks inside `libs` then you're going too far.

Inside libs there are a number of small libraries that are dedicate to specific hardware or groups of functions/structs that are related in some way:

- `can` handles everything CAN Bus related
- `fsm` handles everything finite state machine related (INIT -> SAFE -> ACTIVE -> SLEEP loop)
- `utilities` has a bunch of general utility functions, if you want to introduce a new library function without starting an entirely new library, then utilities is usually a safe bet

### `src/`

This is where the actual hardware specific logic and control logic lives. Usually this is split into `control` which is the primary task of the subsystem/esp32, usually data acquisition, some control flow, or whatever. `comms` is entirely just CAN communication, mostly the heartbeat, responding to commands, and more. These are pretty clearly shown with the `control.c/.h` and `comms.c/.h` convention used throughout the codebase.

There is also `setup.c` this is the main entry point for all ESP32s, think of it as a `main.c` essentially. It contains all the set up code needed, including a `app_main` function and often also the main FreeRTOS tasks for communication and control (which use functions from the relevant headers).

You will notice most of the control, set up, communication code is abract and doesn't feature much hardware interfacing. That's because all hardware specific logic lives within the [Hardware Abstraction Layer (HAL)](https://en.wikipedia.org/wiki/Hardware_abstraction), so ESP32-specific driver code is kept separate from other logic. This is done to make testing easier, as I can run tests locally or on a CI/CD without need ESP32s, but also keeps all the messy hardware specific logic in its own place which I find nicer to work with.

There are 5 folders within `src/`:

- `Master`: for OBDH/TTC; sd card logging, sending data over ethernet, main flight manager, etc
- `EPS`: for the Electric Power Supply (EPS) code; this is mostly just a current and voltage sensor, all the real power shit is buck converters on the main PCB (not a software problem yippeee!)
- `Instrumentation`: this is a bunch of housekeeping sensors, pretty chill, just DAQ
- `Mechanisms`: houses the iris and fluidics code, again pretty easy as it's just motoring
- `Photonics`: DAQ for photodiodes 
- `thermals`: a bit more complex, will be a bang bang controller or pid loop controlling the temperature regulation of the experiment

### `tests/`

This is where all the test files live. Split into `libs/` for testing library functions and `src/` for testing source code functions.
I'm using `GoogleTest`, a C++ framework for writing the tests so that's why you might see:

```c
#ifdef __cplusplus
extern "C" {
#endif

// ... code or smth

#ifdef __cplusplus
}
#endif
```

This is especially common in the `libs` code header code.

### Config files

We use CMake for building and platformio.ini for compiling each ESP32 easily. There's also script files like `build.sh` which are handy for quickly building everything. For that reason I recommend using a unix supported operating system such as MacOS, Linux, or WSL2 (if on Windows) as everything will run the same as on my computer.

## Contributing

If you're interested in contributing to the project, I'm going to write some rough guidelines and ideas for getting started.

### How to start

I'd recommend use MacOS, Linux, or Windows Subsystem for Linux (WSL2) for development. All scripts and configs have been testing on those and it should be easier to start rather than trying to get everything to work on Windows.

You will need to install: `cmake`, `platfromio`, `googletest` (`gtest`), `git`, `doxygen`, and `python` (dependency of platformio).
You can run the `setup.sh` that should install all these packages and sets everything up for you. This is probably the best place to start.

I'd recommend using the `platformio` extension and `vscode` for development if you're new, or any set up you're familiar with if you're more experienced.

#### Git Flow

I've used Git Flow methodology in the past, and although it's a ballache if you're managing a repository on your own, when other people start getting interested in contritbing that's usually when I start using it.
The idea is pretty simple: `main` branch for production ready code, `dev` branch for still in progress code, and individual contributors create branches off of `dev` to write and test code before merging into `dev`. Then merges into `main` are reviewed by myself or another collaborator on the project.

If you're getting an error when pushing to `main` that the branch is protected and you can't push, that is intentional. You should stash your work, open a new branch from `dev`, commit, and pull request into `dev` rather than `main`.

### Where to start

It's hard to know exactly where to start, especially when you're dropped into the middle of a codebase written entirely by someone else.
I'd recommend checking out the issues tab, I will start populating it with work that needs being done.
Otherwise, read through the codebase and decide to add new functionality, add tests for something, or document stuff.

#### Documentation

The code should be fairly self documenting, I'd like to avoid code that is overly complex for no reason, but sometimes it is neccessary or just happens. To avoid this I've added Doxygen to the project (and will be setting up a doxygen web build soon). You will see these through out the codebase as comments, but for a brief summary:

```c
/**
 * @file some_library.h
 * @brief Add a brief description of what's in the library
 */
#ifndef HELIA_SOME_LIBRARY_H
#define HELIA_SOME_LIBRARY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Brief description of the point of this struct */
typedef struct {
    float value;    /**< This value is used for.. Or is in .. format */
    bool  ok;       /**< True if the data is fresh/valid, otherwise disregard */
} some_data;

/**
 * @brief Description of the function's behaviour, 
 * @param f What is this param and how is it relevant
 * @param alpha etc etc
 * 
 * @return this function is void, but this would discuss what the return is 
 */
void some_init(some_data *f, float alpha);

/**
 * @brief Description of the function's behaviour, 
 * @param f What is this param and how is it relevant
 * @param alpha etc etc
 * 
 * @return blah blah blah
 */
float some_func(some_data *f, float sample);

#ifdef __cplusplus
}
#endif

#endif /* HELIA_SOME_LIBRARY_H */

```

The example above should give you an idea of how most of the code is documented now.
Anything else can be found in the [doxygen documentation](https://www.doxygen.nl/manual/docblocks.html) (just skim the code examples tbh).

### Final notes

This repository is for Project HELIA, a BEXUS flight experiment launching in October (2026). After the launch campaign I may not respond as quickly to enquiries or pull requests. You should be able to ping me using @hrszpuk in your issues/pull requests.
However, even then this codebase won't be a top priority for me, as I'll have moved onto other projects.

Happy coding!
