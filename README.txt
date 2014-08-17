Lab 9: Socket Programming
Following are the necessary Project Details and the code has been well documented for explanation.


Directory Structure:
********************
/src : This includes all our source files. (User.cpp and FileMeshNode.cpp)
/config: This includes sample configuration files. (FileMesh1.cfg and FileMesh2.cfg)
/bin: Output executables are formed here.
/receivedFiles: In case the machine runs user program, the received files are stored in this directory.

Compilation Instructions and Running Instructions:
**************************************************
(You have to be present in the main directory for running these commands)

For every unique node:
***************************************************
g++ src/FileMeshNode.cpp -o bin/file
./bin/file config/FileMesh.cfg myNodeId maxNodes


For running the User Program:
*****************************
g++ ./src/User.cpp -o ./bin/user
./bin/user 10.10.30.11 config/FileMesh1.cfg (Here, the first argument is the IP Address of the User Machine and the second argument is the Configuration File)

In case of the User Machine, the received files are stored in /receivedFiles.

Configuration File:
*******************
Sample line in the Configuration File looks like:
10.10.30.15:7020 /home/rj/Documents/algorithms-master/erl/sorting

Here the first parameter is the IP address alongwith port(corresponding to a node) and the second parameter is the location where the files are stored.
