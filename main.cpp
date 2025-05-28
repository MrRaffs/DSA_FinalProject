#include <iostream>

using namespace std;



void quit() {
    //updatePaketDB();
    cout << "\n\nProgram End." << endl;
}

void init() {
    //initPaketDB();
}

int main() {
    atexit(quit);
    // init();
    return 0;
}
