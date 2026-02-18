#include <iostream>
#include <string>
#include "classes.h"
#include <stdexcept>


using namespace std;

int main(int argc, char* argv[]) {

    LinearHashIndex hashIndex("EmployeeIndex.dat");

    // Build index
    hashIndex.createFromFile("Employee.csv");

    // Search IDs passed as arguments
    for (int i = 1; i < argc; i++) {
        int id = stoi(argv[i]);
        cout << "\nSearching for ID: " << id << endl;
        hashIndex.findAndPrintEmployee(id);
    }

    return 0;
}  