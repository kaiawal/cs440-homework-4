
#include <string>
#include <iostream>
#include <stdexcept>
#include "classes.h"

using namespace std;


int main() {

    fstream indexFile("EmployeeIndex.dat", ios::binary | ios::in | ios::out | ios::trunc);
    indexFile.close();

    // Create the index
    HashIndex hashIndex("EmployeeIndex.dat");
    hashIndex.createFromFile("Employee-4_og.csv");

    // Loop to lookup IDs until user is ready to quit
    std::string searchID;
    std::cout << "Enter the employee ID to find or type exit to terminate: ";
    while (std::cin >> searchID && searchID != "exit") {
        int64_t id = std::stoll(searchID);
        std::string record;
        hashIndex.findAndPrintEmployee(id);
    }

    return 0;
}
