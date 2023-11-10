# Janksh

As the name suggest, it's a very janky shell program, with no guarantee about memory leak protection, and functionality (??). 

This was a coursework about building a simple shell program. But I was kinda proud of what I ended up building. Not the prettiest code ngl but nonetheless I had enjoyed building it.

## Functionality

While it is very janky, it does serve some purposes (not much though).

- Command parser. Not a very complicated nor functional one, but it does the job. (And it does assume you type the command quite cleanly. Insert spaces to make it pretty!)
- Command execution. It's actually a bit more complicated than one would've imagine. Which I had a lot of fun understanding it. Modern shells are much more complicated, but in this case `fork() + exec()` did the trick here.
- `^C` Keyboard interruption. Classic.
- Directory navigation. `cd` command is here, the most important command (imo) in a shell program.
- Background job execution. With a little `&` at the end, make the command executing at background while allowing user to typing other commands. `fg`, `bg`, `^Z` are all here as well.
- Job monitoring. Primarily emulating the `jobs` command in bash shell.
- `exit` command. Exits the shell program. Ctrl+D doesn't work for some reason, I had encountered quite some problems detecting ^D (EOF) which sometimes can be mixed up with other character irrc. I hate this limitation but it is what it is for such janky shell. ~~Fuck `ignoreeof`~~
- etc. Tired of listing. :D

# I don't know what title to put here but it's kind of a summary

This project is coursework about a shell program where functionality is the least of its concern. Because its extreme simplicity and jankiness, I believe this actually serves as a perfect project and example for rookies like me to understand some very entry level processes management of Linux, or Unix system in general even.