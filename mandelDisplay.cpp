/* mandelDislay.cpp
 *
 * Mandelbrot Fractals
 *
 * Alex Viznytsya
 * 11/16/2017
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

typedef struct messageBufffer {
    long mtype;
    char mtext[256];
} message;

int NumberOfImagesProcessed = 0;

void SIGUSR1_HANDLER(int signal) {
    exit(NumberOfImagesProcessed);
}

int main(int argc, char* argv[]) {

    int sharedMemoryID = atoi(argv[1]);
    int messageQueueID1 = atoi(argv[2]);
    int messageQueueID2 = atoi(argv[3]);
	
    char symbols[] = {".-~:+*%O8&?$@#X"};
    
    signal(SIGUSR1, SIGUSR1_HANDLER);
   
    int* smData = (int*)shmat(sharedMemoryID, NULL, 0);
    if(smData == (int*)-1) {
        perror("Error: mandelDisplay -> shmat() -> Shared Memory ");
        exit(-11);
    }
    
    while(true) {
        double xMin = 0;
        double xMax = 0;
        double yMin = 0;
        double yMax = 0;
        int nRows = 0;
        int nCols = 0;
        
        fscanf(stdin, "%lf %lf %lf %lf %d %d", &xMin, &xMax, &yMin, &yMax, &nRows, &nCols);
        while ((getchar()) != '\n');
        NumberOfImagesProcessed += 1;
        
        message incomeMandelbrotMessage = {1, ""};
        msgrcv(messageQueueID2, &incomeMandelbrotMessage, 256, 1, IPC_NOWAIT);
        
        // Save Mandelbrot fractal to file for feature visualizations:
        std::string saveFilename = incomeMandelbrotMessage.mtext;
        if (saveFilename != "empty") {
            std::fstream saveFile(incomeMandelbrotMessage.mtext, saveFile.trunc | saveFile.in | saveFile.out);
            for(int r = (nRows - 1); r >= 0; r--) {
                for(int c = 0; c <= (nCols - 1); c++) {
                    int n = *(smData + r * nCols + c);
                    if (n < 0) {
                        saveFile << "-1 ";
                    } else {
                        saveFile << n << " ";
                    }
                }
                saveFile << "\r\n";
            }
            saveFile.close();
        }
        
        // Print Mandelbrot fractal on screen:
        std::cout << std::endl;
        for(int r = (nRows - 1); r >= 0; r--) {
            if(r == (nRows - 1)) {
                printf( "%6.3lf", yMax);
            } else if (r == 0){
                printf( "%-6.3lf", yMin);
            } else {
                printf( "%-6s", "");
            }
            for(int c = 0; c <= (nCols - 1); c++) {
                int n = *(smData + r * nCols + c);
                if (n < 0) {
                    std::cout << " ";
                } else {
                    std::cout << symbols[n % 15];
                }
            }
            std::cout << std::endl;
        }
        printf("%-6s", "");
        printf("%-6.3lf", xMin);
        for(int i  = 0; i < (nCols - 12); i++) {
            std::cout << " ";
        }
        printf("%6.3lf", xMax);
        std::cout << std::endl << std::endl;
        
        
        // Send done message to the parent:
        message newMessage = {2, "Done"};
        if(msgsnd(messageQueueID1, &newMessage, 13, IPC_NOWAIT) < 0) {
            perror("\nError: mandelDisplay -> msgsnd(\"Done\") -> Message Queue 1 ");
            exit(-9);
        }
    }

    return NumberOfImagesProcessed;
}
