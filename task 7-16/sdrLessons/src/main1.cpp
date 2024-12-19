#include <iio/iio.h>
#include <iio/iio-debug.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include "./myfile.h" 

const int SAMPLE_COUNT = sizeof(x_bb_real) / sizeof(x_bb_real[0]);

struct sigaction old_action;

void sigint_handler(int sig_no) {
    printf("CTRL-C pressed\n");
    sigaction(SIGINT, &old_action, NULL);
    exit(0);
}

int main() {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);

    
    std::ofstream outfile("output.txt");
    if (!outfile.is_open()) {
        std::cerr << "Unable to open file for writing" << std::endl;
        return 1;
    }

    
    try {
        
        for (int i = 0; i < SAMPLE_COUNT; i++) {
            double real = x_bb_real[i];
            double imag = x_bb_imag[i];

            
            outfile << real << "," << imag << "\n";
        }
    } catch (const std::exception &e) {
        std::cout << "An error occurred: " << e.what() << std::endl;
    }

    
    outfile.close();
    

    return 0;
}