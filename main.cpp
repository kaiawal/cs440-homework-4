
#include <string>
#include <iostream>
#include <stdexcept>
#include "classes.h"

using namespace std;


int main() {

    // Create the index
    HashIndex hashIndex("EmployeeIndex.dat");
    hashIndex.createFromFile("Employee-4.csv");

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
