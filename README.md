# Directory-Watcher

File:   inotify.cpp
Date:   June 23, 2017
Author: Mayank Patel <mayankmca90@gmail.com>, on the shoulders of many others

This is a simple inotify sample program monitoring changes to "./Directory-Watcher" directory (create ./Directory-Watcher beforehand)
Recursive monitoring of file and directory create and delete events is implemented, but
monitoring pre-existing "./Directory-Watcher" subfolders is not.
A C++ class containing a couple of maps is used to simplify monitoring.
The Watch class is minimally integrated, so as to leave the main inotify code 
easily recognizeable. 
 
 *N.B.* 
 1. This code is meant to illustrate inotify usage, and not intended to be 
    production ready. Exception and error handling is largely incomplete or missing.
 2. inotify has a fundamental flaw for which there is no real solution: if subdirectories are
    created too quickly, create events will be lost, resulting in those subdirectories (as well
    as their children) going unwatched. 
 3. fanotify, available in newer kernels, can monitor entire volumes, and is often a better solution.
    
 
 This code sample is released into the Public Domain.

 
 To compile:
    $ g++ inotify.cpp -o inotify

 To run:
    $ ./inotify
 
 To exit:
    control-C
