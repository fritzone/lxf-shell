

# What the shell

Here are some key points we need to understand about the shell, and how these notions will be incorporated into our shell that we will develop.

1. **Command Interpretation**: There are two categories of commands that we will need to support:
   1. External commands (`ls`, `date`) are executable files that are already residing in the directories, running them is the theme of the current episode.   
   2. Support for built-in commands (`cd`, `alias`, etc...) will be provided using the backbone plugin architecture we will introduce soon.
2. **Redirection and Pipelines**: Introduction into developing the shell's support for features like input/output redirection and pipelines will be started in our next episode.
3. **Shell Prompt**: Customizing the shell prompt will be an important part of our series. We will add features for displaying information like hostname, current directory, and more. 
4. **History and Command Line Editing**: Since the author of the article is really unhappy with some features offered by current shell implementations concerning history, we will introduce a rather novel approach to how history will be handled by our shell.
5. **Tab Completion**: Pressing the "Tab" key has its special place in our hearts and we will present an innovative approach on how to make this feature even more widely used by introducing a support library for command completion.
6. **Customization**: Implementing variables and aliases will be introduced very soon in the article series, however support for functions will be delayed till we have the scripting feature in its place.
7. **Scripting**: Implementing a scripting language specific to our shell will be revisited at a later stage due to the sheer size of the project.

And as a final touch, in homage to the magazine that accepted the publication of the author's blusterings, we shall call the shell **lxf-shell**.