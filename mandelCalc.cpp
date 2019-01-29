/* mandelCalc.cpp
 *
 * Mandelbrot Fractals
 *
 * Alex Viznytsya
 * 11/16/2017
 */

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

typedef struct messageBuffer {
    long mtype;
    char mtext[256];
} message;

int NumberOfImagesProcessed = 0;

void SIGUSR1_HANDLER(int signal) {
    exit(NumberOfImagesProcessed);
}

int main(int argc, char* argv[]) {

    int sharedMemoryID = atoi(argv[1]);
    int messageQueueID = atoi(argv[2]);
    
    signal(SIGUSR1, SIGUSR1_HANDLER);
    
    int* smData = (int*)shmat(sharedMemoryID, NULL, 0);
    if(smData == (int*)-1) {
        perror("Error: mandelCalc -> shmat() -> Shared Memory ");
        exit(-11);
    }
    while(true) {
        
        double xMin = 0;
        double xMax = 0;
        double yMin = 0;
        double yMax = 0;
        int nRows = 0;
        int nCols = 0;
        int maxIters = 0;
        
        fscanf(stdin, "%lf %lf %lf %lf %d %d %d", &xMin, &xMax, &yMin, &yMax, &nRows, &nCols, &maxIters);
        while ((getchar()) != '\n');
        NumberOfImagesProcessed += 1;
        
        // Mandelbrot Algorithm:
        double deltaX = (xMax - xMin) / (nCols - 1);
        double deltaY = (yMax - yMin) / (nRows - 1);
        
        for (int r = 0; r <= (nRows - 1); r++) {
            double Cy = yMin + r * deltaY;
            for (int c = 0; c <= (nCols - 1); c++) {
                double Cx = xMin + c * deltaX;
                double Zx = 0.0;
                double Zy = 0.0;
                int n = 0;
                for (n = 0; n < maxIters; n++) {
                    if (Zx * Zx + Zy * Zy >= 4.0) {
                        break;
                    }
                    double Zx_next = Zx * Zx - Zy * Zy + Cx;
                    double Zy_next = 2.0 * Zx * Zy + Cy;
                    Zx = Zx_next;
                    Zy = Zy_next;
                }
                if (n >= maxIters) {
                    *(smData + r * nCols + c) = -1;
                } else {
                    *(smData + r * nCols + c) = n;
                }
            }
        }
        
        // Send user data to mandelDisplay via pipe:
        fprintf(stdout, "%lf %lf %lf %lf %d %d\n", xMin, xMax, yMin, yMax, nRows, nCols);
        fflush(stdout);
        
        // Send done message to the parent:
        message newMessage = {1, "Done"};
        if(msgsnd(messageQueueID, &newMessage, 13, IPC_NOWAIT) < 0) {
            perror("\nError: mandelCalc -> msgsnd(\"Done\") -> Message Queue 1 ");
            exit(-9);
        }
    }
    
    return NumberOfImagesProcessed;
}
