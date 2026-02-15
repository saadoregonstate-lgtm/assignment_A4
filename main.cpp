#include <string>
#include <iostream>
#include <stdexcept>
#include "classes.h"

using namespace std;

int main(int argc, char* argv[]) {

    LinearHashIndex hashIndex("EmployeeIndex.dat");

    hashIndex.createFromFile("Employee.csv");

    if (argc < 2) {
        cout << "Please provide employee IDs as command-line arguments." << endl;
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        try {
            int empId = stoi(argv[i]);
            cout << "Searching for Employee ID: " << empId << endl;
            hashIndex.findAndPrintEmployee(empId);
        }
        catch (invalid_argument&) {
            cout << "Invalid employee ID: " << argv[i] << endl;
        }
    }

    return 0;
}