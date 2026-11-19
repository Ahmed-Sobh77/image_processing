# Parallel Image Processing using MPI

A parallel image processing application developed using **C** and **MPI (Message Passing Interface)**. The project distributes image processing tasks across multiple processes to perform image transformations efficiently.

## Feature

- **Grayscale:** Converts images to grayscale.
- **Brightness:** Adjusts image brightness.
- **Contrast:** Enhances image contrast.
- **Blur:** Applies a blur filter to images.
- **Parallel Processing:** Distributes image processing tasks across multiple MPI processes.

## Technologies Used

- C
- MPI (Microsoft MPI)
- CMake
- P3 (ASCII PPM Image Format)

## Prerequisites

Before running the project, make sure you have installed:

- [Git](https://git-scm.com/)
- [CMake](https://cmake.org/download/)
- [CLion](https://www.jetbrains.com/clion/) or another CMake-compatible IDE.
- A C compiler.
- Microsoft MPI Runtime and SDK.

## Installation

### 1. Clone the repository

```bash
git clone https://github.com/Ahmed-Sobh77/image_processing.git
cd image_processing
```

### 2. Install Microsoft MPI

Download and install both:

- Microsoft MPI Runtime
- Microsoft MPI SDK

Make sure MPI is properly configured on your system.

### 3. Build the project

Open the project folder in CLion.

CLion will automatically detect `CMakeLists.txt` and configure the project.

Build the project using the Build option in CLion.

### 4. Run the project

Open a terminal in the project directory and execute:

```powershell
mpiexec -n 4 .\cmake-build-debug\taskkkkk.exe
```

You can change `4` to the number of MPI processes you want to run.

## Input and Output

- **Input:** P3 (ASCII PPM) image files.
- **Output:** Processed P3 image files.

P3 stores image pixel data as readable ASCII numbers, making it easy to inspect and process image data.

## Learning Objectives

This project demonstrates practical applications of parallel computing, including:

- Parallel task distribution.
- MPI process communication.
- Workload distribution across multiple processes.
- Gathering processed image data.
- Applying image processing algorithms in a parallel environment.
