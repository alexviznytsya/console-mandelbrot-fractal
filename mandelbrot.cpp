/* mandelbrot.cpp
 *
 * Mandelbrot Fractals
 *
 * Alex Viznytsya
 * 11/16/2017
 */

#define READ 0
#define WRITE 1

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string>
#include <cstring>
#include <cstdint>


// Important global variables:
typedef struct messageBuffer {
    long mtype;
    char mtext[256];
} message;

pid_t mandelCalcChildPID = 0;
pid_t mandelDisplayChildPID = 0;
int mandelCalcChildStatus = 0;
int mandelDisplayChildStatus = 0;
int SharedMemoryID = 0;
int MessageQueueID1 = 0;
int MessageQueueID2 = 0;

// Free shared resources functions:
void closeSharedMemory() {
    shmctl(SharedMemoryID, IPC_RMID, NULL);
}

void closeMessageQueue1() {
    msgctl(MessageQueueID1, IPC_RMID, NULL);
}

void closeMessageQueue2() {
    msgctl(MessageQueueID2, IPC_RMID, NULL);
}
void closeResources() {
    closeSharedMemory();
    closeMessageQueue1();
    closeMessageQueue2();
}

// Signal handlers:
void SIGCHLD_HANDLER(int signal) {
    
    waitpid(mandelCalcChildPID, &mandelCalcChildStatus, WNOHANG);
    waitpid(mandelDisplayChildPID, &mandelDisplayChildStatus, WNOHANG);
    
    int8_t conMandelChildStatus = WEXITSTATUS(mandelCalcChildStatus);
    int8_t conMandelDisplayStatus = WEXITSTATUS(mandelDisplayChildStatus);
    
    if(conMandelChildStatus < 0) {
        closeResources();
        std::cerr << std::endl << "Error: Child mandelCalc exited with error: "<< (int)conMandelChildStatus <<".\n       Terminating mandelDisplay and exit." << std::endl;
        if(kill(mandelDisplayChildPID, SIGKILL) > 0) {
            perror("\nError: Mandelbrot -> SIGCHKD_HANDLER -> kill(9) -> mandelDisplay ");
            exit(-8);
        }
        exit(-15);
    }
    
    if(conMandelDisplayStatus < 0) {
        closeResources();
        std::cout << std::endl << "Error: Child mandelDisplay exited with error: "<< (int)conMandelDisplayStatus <<".\n       Terminating mandelCalc and exit." << std::endl;
        if(kill(mandelCalcChildPID, SIGKILL) > 0) {
            perror("\nError: Mandelbrot -> SIGCHKD_HANDLER -> kill(9) -> mandelCalc ");
            exit(-8);
        }
        exit(-15);
    }
}

void SIGINT_HANDLER(int signal) {
    closeResources();
    std::cerr << std::endl << "crtc-c key has been detected. Freeing resources and exiting ..." << std::endl;
    exit(-14);
}

// Print text functions:
void printHelp() {
    std::cout << "Input help:" << std::endl;
    std::cout << "  xMIn, xMax   - Defining the X range of the complex to be displayed." << std::endl;
    std::cout << "                 X is from -2.0 to 2.0." << std::endl;
    std::cout << "  yMIn, yMax   - Defining the Y range of the complex to be displayed." << std::endl;
    std::cout << "                 Y is from -1.5 to 1.5." << std::endl;
    std::cout << "  nRows, nCols - Defining how many chars tall and wide the image will be." << std::endl;
    std::cout << "  maxIter      - Maximum number of itteration before calculation result" << std::endl;
    std::cout << "                 will be considered in Mandelbrot set." << std::endl << std::endl;
}

void printMyInfo() {
    std::cout << "Homework Assignment 4" << std::endl;
    std::cout << "Inter-Process Communication & I/O" << std::endl;
    std::cout << "CS-361 - Computer Systems, Fall 2017" << std::endl <<std::endl;
    std::cout << "Alex Viznytsya" << std::endl;
    std::cout << "NetId: avizny2" << std::endl;
    std::cout << "11/16/2017" << std::endl << std::endl;
}


int main(int argc, char *argv[]) {

    printMyInfo();
    
    // Initialization of program variables:
    int mandelCalcPipe[2];
    int mandelDisplayPipe[2];
    
    // Create pipes for childten communication:
    if (pipe(mandelCalcPipe) < 0) {
        perror("\nError: Mandelbrot -> pipe() -> mandelCalc ");
        exit(-1);
    }
    if (pipe(mandelDisplayPipe) < 0) {
        perror("\nError: Mandelbrot -> pipe() -> mandelDisplay ");
        exit(-1);
    }
    
    // Create message queues:
    if((MessageQueueID1 = msgget(IPC_PRIVATE, 0600 | IPC_CREAT)) < 0) {
        perror("\nError: Mandelbrot -> msgget() -> Message Queue 1 ");
        exit(-2);
    }
    
    if((MessageQueueID2 = msgget(IPC_PRIVATE, 0600 | IPC_CREAT)) < 0) {
        closeMessageQueue1();
        perror("\nError: Mandelbrot -> msgget() -> Message Queue 2 ");
        exit(-2);
    }
    
    //Initialization of custom signal Handlers:
    signal(SIGCHLD, SIGCHLD_HANDLER);
    signal(SIGINT, SIGINT_HANDLER);
    
    // Create shared memory:
    if((SharedMemoryID = shmget(IPC_PRIVATE, 40000, 0600 | IPC_CREAT)) < 0) {
        closeMessageQueue1();
        closeMessageQueue2();
        perror("\nError: Mandelbrot -> shmget() -> Shared Memory ");
        exit(-3);
    }
    
	char shmIDText[32];
	sprintf(shmIDText, "%d", SharedMemoryID);
	char mqID1Text[32];
	sprintf(mqID1Text, "%d", MessageQueueID1);
	char mqID2Text[32];
	sprintf(mqID2Text, "%d", MessageQueueID2);
	
    // Fork two children:
    if ((mandelCalcChildPID = fork()) == 0) {
        
        //Close 1st child pipes:
        if(close(mandelCalcPipe[WRITE]) < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> close() -> pipe([write]) -> mandelCalc ");
            exit(-4);
        }
        if(close(mandelDisplayPipe[READ]) < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> close() -> pipe([read]) -> mandelDisplay ");
            exit(-4);
        }
        
        // Changing stdin and stdout in 1st child;
        if(dup2(mandelCalcPipe[READ], 0) < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> dup2() -> pipe([read]) -> mandelCalc -> 0 ");
            exit(-5);
        }
        if(dup2(mandelDisplayPipe[WRITE], 1) < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> dup2() -> pipe([write]) -> mandelDisplay -> 1 ");
            exit(-5);
        }

        // Close duplicated pipes:
        if(close(mandelCalcPipe[READ]) < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> close() -> pipe([read]) -> mandelCalc ");
            exit(-4);
        }
        if(close(mandelDisplayPipe[WRITE]) < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> close() -> pipe([write]) -> mandelDisplay ");
            exit(-4);
        }
        
        // Run mandelCalc program:
        char* mandelCalcChildArguments[] = {(char*)"./mandelCalc",
                                            shmIDText,
                                            mqID1Text,
                                            NULL};
        execvp(mandelCalcChildArguments[0], mandelCalcChildArguments);
        closeResources();
        perror("\nError: Mandelbrot -> execvp() -> mandelCalc ");
        exit(-6);
    } else if (mandelCalcChildPID < 0){
        closeResources();
        perror("\nError: Mandelbrot -> fork() -> mandelCalc ");
        exit(-7);
    } else {
        if((mandelDisplayChildPID = fork()) == 0) {
        
            //Close 2nd child pipes:
            if(close(mandelCalcPipe[READ]) < 0) {
                closeResources();
                perror("\nError: Mandelbrot -> close() -> pipe([read]) -> mandelCalc ");
                exit(-4);
            }
            if(close(mandelCalcPipe[WRITE]) < 0) {
                closeResources();
                perror("\nError: Mandelbrot -> close() -> pipe([write]) -> mandelCalc ");
                exit(-4);
            }
            if(close(mandelDisplayPipe[WRITE]) < 0) {
                closeResources();
                perror("\nError: Mandelbrot -> close() -> pipe([write]) -> mandelDisplay ");
                exit(-4);
            }
            
            // Changing stdin in 2nd child:
            if(dup2(mandelDisplayPipe[READ], 0) < 0) {
                closeResources();
                perror("\nError: Mandelbrot -> dup2() -> pipe([read]) -> mandelDisplay -> 0 ");
                exit(-5);
            }
            
            // Close duplicated pipes:
            if(close(mandelDisplayPipe[READ]) < 0) {
                closeResources();
                perror("\nError: Mandelbrot -> close() -> pipe([read]) -> mandelDisplay ");
                exit(-4);
            }
        
            // Run mandelDisplay program:
			char *mandelDisplayChildArguments[] = {(char*)"./mandelDisplay",
                                                   shmIDText,
                                                   mqID1Text,
                                                   mqID2Text,
                                                   NULL};
            execvp(mandelDisplayChildArguments[0], mandelDisplayChildArguments);
            closeResources();
            perror("\nError: Mandelbrot -> execvp() -> mandelDisplay ");
            exit(-1);
        } else if(mandelDisplayChildPID < 0) {
            closeResources();
            perror("\nError: Mandelbrot -> fork() -> mandelDisplay ");
            exit(-7);
        }
    }	

    // Close parent pipes:
    if(close(mandelCalcPipe[READ]) < 0) {
        closeResources();
        perror("\nError: Mandelbrot -> close() -> pipe([read]) -> mandelCalc ");
        exit(-4);
    }
    if(close(mandelDisplayPipe[READ]) < 0) {
        closeResources();
        perror("\nError: Mandelbrot -> close() -> pipe([read]) -> mandelDisplay ");
        exit(-4);
    }
    if(close(mandelDisplayPipe[WRITE]) < 0) {
        closeResources();
        perror("\nError: Mandelbrot -> close() -> pipe([write]) -> mandelDisplay ");
        exit(-4);
    }
    
    // Create FILE* type variable for feature using of fprintf():
    FILE* outputToMandelCalc = fdopen(mandelCalcPipe[WRITE], "w");
    
    std::cout << "Welcome to Mandelbrot fractal image creator." << std::endl << std::endl;
    
    // Main loop:
    bool resumeLoop = true;
    bool programExit = false;
    
    while(true) {
        
        double xMin = 0;
        double xMax = 0;
        double yMin = 0;
        double yMax = 0;
        int nRows = 0;
        int nCols = 0;
        int maxIters = 0;
        std::string userInput;
        std::string saveFilename;
        std::string saveFileAnswer;
        std::string overrideAnswer;
        
        // Ask user to input data:
        if(resumeLoop == true) {
            resumeLoop = false;
            std::cout << "Please enter xMin, xMax, yMin, yMax, nRows, nCols, and maxIter" << std::endl;
            std::cout << "separated by space, or type \"exit\" to exit or \"help\" for help:" << std::endl;
            std::cout << "> ";
            getline(std::cin, userInput);
            if (userInput.find("exit") != std::string::npos) {
                programExit = true;
            }
            if (userInput.find("help") != std::string::npos) {
                printHelp();
                resumeLoop = true;
                userInput.clear();
                continue;
            }
        
        // Ask if user wants to create new fractal:
        } else {
            std::string continueAnswer;
            std::cout << "Do you want to create another Mandelbrot fractal? (y/n) > ";
            std::cin >> continueAnswer;
            std::cin.ignore(INT_MAX,'\n');
            while(continueAnswer != "y" && continueAnswer != "n") {
                std::cout << "You have entered wrong answer. Please enter y/n >";
                std::cin >> continueAnswer;
                std::cin.ignore(INT_MAX,'\n');
            }
            if(continueAnswer == "n") {
                programExit = true;
            } else {
                resumeLoop = true;
                userInput.clear();
                continue;
            }
        }
        
        // Check if user wants to continue using programm:
        if(programExit == false) {
            
            // Check if previous input is not empty:
            if(userInput.length() == 0) {
                std::cout << "Input cannot be empty. Please try again." << std::endl;
                resumeLoop = true;
                continue;
            }
            
            // Get data from stdin to variables:
            if (sscanf(userInput.c_str(), "%lf %lf %lf %lf %d %d %d", &xMin, &xMax, &yMin, &yMax, &nRows, &nCols, &maxIters) == 0) {
                std::cout << "Wrong input. Please try again." << std::endl;
                resumeLoop = true;
                continue;
            }
    
            // Error checking:
            if((xMin < -2.0 || xMin > 2.0) || (xMax < -2.0 || xMax > 2.0)) {
                std::cout << "xMin and xMax must be in range -2.0 to 2.0. Please try again." << std::endl;
                resumeLoop = true;
                continue;
            }
            if((yMin < -1.5 || yMin > 1.5) || (yMax < -1.5 || yMax > 1.5)) {
                std::cout << "xMin and xMax must be in range -1.5 to 1.5. Please try again." << std::endl;
                resumeLoop = true;
                continue;
            }
            if(nRows < 1 || nCols < 1 || maxIters < 1) {
                std::cout << "nRows, nCols or maxIter must be greater than 0. Please try again." << std::endl;
                resumeLoop = true;
                continue;
            }
            
            // Ask if user wants to save fractal to file:
            while(true) {
                if(saveFileAnswer != "y") {
                    std::cout << "Do you want to save result to text file? (y/n) > ";
                    std::cin >> saveFileAnswer;
                    std::cin.ignore(INT_MAX,'\n');
                }
                while(saveFileAnswer != "y" && saveFileAnswer != "n") {
                    std::cout << "You have entered wrong answer. Please enter y/n >";
                    std::cin >> saveFileAnswer;
                    std::cin.ignore(INT_MAX,'\n');
                }
                if(saveFileAnswer == "n") {
                    saveFileAnswer.clear();
                    break;
                } else {
                    std::cout << "Please enter filename with .txt extension > ";
                    std::cin >> saveFilename;
                    std::cin.ignore(INT_MAX,'\n');
                    while (saveFilename.find(".txt") == std::string::npos) {
                        std::cout << "You have not entered .txt extension. Please try again > ";
                        std::cin >> saveFilename;
                        std::cin.ignore(INT_MAX,'\n');
                    }
                    std::ifstream textFile(saveFilename.c_str());
                    if (!textFile.good()) {
                        saveFileAnswer.clear();
                        break;
                    } else {
                        std::cout << "File \""<< saveFilename <<"\" already exists. Do you want to override it? (y/n) > ";
                        std::cin >> overrideAnswer;
                        std::cin.ignore(INT_MAX,'\n');
                        while(overrideAnswer != "y" && overrideAnswer != "n") {
                            std::cout << "You have entered wrong answer. Please enter y/n >";
                            std::cin >> overrideAnswer;
                            std::cin.ignore(INT_MAX,'\n');
                        }
                        if(overrideAnswer == "y") {
                            overrideAnswer.clear();
                            break;
                        } else if (overrideAnswer == "n") {
                            saveFileAnswer = "y";
                            overrideAnswer.clear();
                            saveFilename.clear();
                            continue;
                        }
                    }
                }
            }
            
            
            // Send save filename to mandelDisplay via Message Queue 2:
            message newMessage = {1, "empty"};
            if (saveFilename.length() != 0) {
                strcpy(newMessage.mtext, saveFilename.c_str());
				
            }
			
            if(msgsnd(MessageQueueID2, &newMessage, (strlen(newMessage.mtext) + 9), IPC_NOWAIT) < 0) {
                closeResources();
                perror("\nError: Mandelbrot -> msgsnd() -> Message Queue 2 ");
                exit(-9);
            }
            
			// Send user data to mandelCalc via pipe:
            fprintf(outputToMandelCalc, "%lf %lf %lf %lf %d %d %d\n", xMin, xMax, yMin, yMax, nRows, nCols, maxIters);
            fflush(outputToMandelCalc);
			
            // Listen for done messages from both children
            bool mandelCalcChildDone = false;
            bool mandelDisplayChildDone = false;
            while(true) {
                message incomingManelCalcMessage = {0, "empty"};
                message incomingMandelDisplayMessage = {0, "empty"};
                if (msgrcv(MessageQueueID1, &incomingManelCalcMessage, 256, 1, IPC_NOWAIT) > 0) {
                    mandelCalcChildDone = true;
                }

                if (msgrcv(MessageQueueID1, &incomingMandelDisplayMessage, 256, 2, IPC_NOWAIT) > 0) {
                    mandelDisplayChildDone = true;
                }

                if (mandelCalcChildDone == true && mandelDisplayChildDone == true) {
                    break;
                }
            }
            
        } else {
            std::cout << std::endl << "Exiting program ..." << std::endl;
            
            fclose(outputToMandelCalc);
            closeResources();
            
            // Kill all child
            if(kill(mandelCalcChildPID, SIGUSR1) > 0) {
                closeResources();
                perror("\nError: Mandelbrot -> kill(30) -> mandelCalc ");
                exit(-8);
            }
            if(kill(mandelDisplayChildPID, SIGUSR1) > 0) {
                closeResources();
                perror("\nError: Mandelbrot -> kill(30) -> mandelDisplay ");
                exit(-8);
            }
        
            //Wait for children to exit:
            waitpid(mandelCalcChildPID, &mandelCalcChildStatus, WUNTRACED);
            waitpid(mandelDisplayChildPID, &mandelDisplayChildStatus, WUNTRACED);
            
            printf("mondelCalc exited with status: %d\n", (int8_t)WEXITSTATUS(mandelCalcChildStatus));
            printf("mondelDisplay exited with status: %d\n", (int8_t)WEXITSTATUS(mandelDisplayChildStatus));
            
            std::cout << std::endl << "Thank you for using this program. Bye-bye." << std::endl;
            return 0;
        }
    }
    
    return 0;
}
