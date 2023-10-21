# The making of CMake

Here are some of the main key aspects and features of **CMake**:

1. **Cross-Platform:** **CMake** successfully addresses the challenges of building software on different platforms (e.g., Windows, macOS, Linux) with various build tools (e.g., make, Visual Studio, Xcode) by providing identical approach to the process on all platforms. It generates platform-specific build files (such as Makefiles, project files, or solutions) based on a single set of CMake configuration files.
2. **Configuration Management:** **CMake** allows you to define project configuration settings, such as compiler options, build flags, and custom variables, in a human-readable `CMakeLists.txt` file. This simplifies the process of configuring and managing your projects, regardless of their level of complexity.
3. **Modularity:** **CMake** encourages a modular project structure. You can create reusable **CMake** modules and functions and use them in multiple projects to share common build tasks.
4. **Dependency Management:** **CMake** facilitates the management of project dependencies. You can specify external libraries or packages that your project relies on, and **CMake** magically finds them and links to them during the build process.
5. **Generator System:** **CMake** uses a generator system to create platform-specific build files. Developers can specify the generator they want to use when configuring a project. 
6. **Out-of-Source Builds:** **CMake** promotes out-of-source builds, meaning the build artifacts are generated in a separate directory from the source code. This keeps the source directory clean and makes it easier to switch between different build configurations.
7. **Extensibility:** **CMake** provides a scripting language that allows you to define custom build rules or perform custom configuration tasks, making it highly extensible.
8. **Integration:** Many integrated development environments (IDEs) and build tools support **CMake**, which simplifies the integration of **CMake**-based projects into various development workflows.

