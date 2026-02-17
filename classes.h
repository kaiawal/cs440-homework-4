#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <istream>

using namespace std;

class Record {
public:
    int id, manager_id; // Employee ID and their manager's ID
    string bio, name; // Fixed length string to store employee name and biography

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }
    
    // Default constructor for empty records
    Record() : id(0), manager_id(0) {}
    
    // Function to get the size of the record
    int get_size() const {
        // sizeof(int) is for name/bio size() in serialize function
        return sizeof(id) + sizeof(manager_id) + sizeof(int) + name.size() + sizeof(int) + bio.size();
    }

    // Function to serialize the record for writing to file
    string serialize() const {
        ostringstream oss;
        oss.write(reinterpret_cast<const char *>(&id), sizeof(id));
        oss.write(reinterpret_cast<const char *>(&manager_id), sizeof(manager_id));
        int name_len = name.size();
        int bio_len = bio.size();
        oss.write(reinterpret_cast<const char *>(&name_len), sizeof(name_len));
        oss.write(name.c_str(), name.size());
        oss.write(reinterpret_cast<const char *>(&bio_len), sizeof(bio_len));
        oss.write(bio.c_str(), bio.size());
        return oss.str();
    }

    void print() {
        cout << "\tID: " << id << "\n";
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};

class Page {
public:
    vector<Record> records; // Data_Area containing the records
    vector<pair<int, int>> slot_directory; // Slot directory containing offset and size of each record
    int cur_size = sizeof(int); // Current size of the page including the overflow page pointer. if you also write the length of slot directory change it accordingly.
    int overflowPointerIndex;  // Initially set to -1, indicating the page has no overflow page. 
							   // Update it to the position of the overflow page when one is created.

    // Constructor
    Page() : cur_size(sizeof(int)), overflowPointerIndex(-1) {}

    // Function to clear
    void clear() {
        records.clear();
        slot_directory.clear();
        cur_size = sizeof(int);
        overflowPointerIndex = -1;
    }

    // Function to insert a record into the page
    bool insert_record_into_page(const Record &r) {
        int record_size = r.get_size();
        int slot_size = sizeof(int) * 2;
        if (cur_size + record_size + slot_size > 4096) { // Check if page size limit exceeded, considering slot directory size
            return false; // Cannot insert the record into this page
        } else {
            // Calculate offset for new record (sum of sizes of existing records)
            int offset = 0;
            for (const auto &rec : records) {
                offset += rec.get_size();
            }
            
            records.push_back(r);

            // TODO: update slot directory information ✓
            slot_directory.push_back({offset, record_size});
            cur_size += record_size + slot_size;
            
            return true;
        }
    }

    // Function to write the page to a binary output stream. You may use
    void write_into_data_file(ostream &out) const {
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;

        // Write records into page_data buffer
        for (const auto &record : records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // TODO: ✓ (Debug)
        //  - Write slot_directory in reverse order into page_data buffer.
        //  - Write overflowPointerIndex into page_data buffer.
        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.
        
        // Write slot directory in reverse order at the end of the page
        int current_pos = 4096;
        
        // Write overflow pointer (4 bytes)
        current_pos -= sizeof(overflowPointerIndex);
        memcpy(page_data + current_pos, &overflowPointerIndex, sizeof(overflowPointerIndex));
        
        // Write number of slots (4 bytes)
        int slot_count = slot_directory.size();
        current_pos -= sizeof(slot_count);
        memcpy(page_data + current_pos, &slot_count, sizeof(slot_count));
        
        // Write slot entries in reverse order
        for (int i = slot_directory.size() - 1; i >= 0; i--) {
            current_pos -= sizeof(pair<int, int>);
            memcpy(page_data + current_pos, &slot_directory[i], sizeof(pair<int, int>));
        }

        out.write(page_data, 4096);
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream
        
        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            // TODO: Process data to fill the records, slot_directory, and overflowPointerIndex
            // Clear existing data
            clear();

            // copying overflow pointer from page to variable
            int current_pos = 4096 - sizeof(overflowPointerIndex);
            memcpy(&overflowPointerIndex, page_data + current_pos, sizeof(overflowPointerIndex));

            // grabbing size of slot directory
            current_pos -= sizeof(int);
            int slot_count;
            memcpy(&slot_count, page_data + current_pos, sizeof(slot_count));

            // Read slot directory (reading backwards)
            slot_directory.resize(slot_count);
            for (int i = slot_count - 1; i >= 0; i--) {
                current_pos -= sizeof(pair<int, int>);
                memcpy(&slot_directory[i], page_data + current_pos, sizeof(pair<int, int>));
            }

            // Read records using slot directory
            for (const auto &slot : slot_directory) {
                int offset = slot.first;
                
                // Read id
                int id;
                memcpy(&id, page_data + offset, sizeof(id));
                offset += sizeof(id);
                
                // Read manager_id
                int manager_id;
                memcpy(&manager_id, page_data + offset, sizeof(manager_id));
                offset += sizeof(manager_id);
                
                // Read name
                int name_len;
                memcpy(&name_len, page_data + offset, sizeof(name_len));
                offset += sizeof(name_len);
                string name(page_data + offset, name_len);
                offset += name_len;
                
                // Read bio
                int bio_len;
                memcpy(&bio_len, page_data + offset, sizeof(bio_len));
                offset += sizeof(bio_len);
                string bio(page_data + offset, bio_len);
                
                // Create record
                vector<string> fields;
                fields.push_back(to_string(id));
                fields.push_back(name);
                fields.push_back(bio);
                fields.push_back(to_string(manager_id));
                
                records.emplace_back(fields);
            }

            // Recalculate cur_size
            cur_size = sizeof(int); // Start with overflow pointer space
            for (size_t i = 0; i < records.size(); i++) {
                cur_size += records[i].get_size() + sizeof(int) * 2; // Record size + slot entry size
            }
            return true;
        }
        
        if (bytes_read > 0) {
            cerr << "Incomplete read: Expected 4096 bytes, but only read " << bytes_read << " bytes." << endl;
        }

        return false;
    }    
};

class HashIndex {
private:
    const size_t maxCacheSize = 1; // Maximum number of pages in the buffer
    const int Page_SIZE = 4096; // Size of each page in bytes
    vector<int> PageDirectory; // Map h(id) to a bucket location in EmployeeIndex(e.g., the jth bucket)
    // can scan to correct bucket using j*Page_SIZE as offset (using seek function)
    // can initialize to a size of 256 (assume that we will never have more than 256 regular (i.e., non-overflow) buckets)
    int nextFreePage; // Next place to write a bucket
    string fileName;

    // Function to compute hash value for a given ID
    int compute_hash_value(int id) {
        int hash_value;

        // TODO: Implement the hash function h = id mod 2^8 ✓
        hash_value = id % (int)pow(2, 8);
        return hash_value;
    }

    // Function to add a new record to an existing page in the index file
    void addRecordToIndex(int pageNum, Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }

        // TODO: ✓
        //  - Use seekp() to seek to the offset of the correct page in the index file
		//		indexFile.seekp(pageIndex * Page_SIZE, ios::beg); ✓
		//  - try insert_record_into_page()
		//     - if it fails, then you'll need to either...
		//			- go to next overflow page and try inserting there (keep doing this until you find a spot for the record) ✓
		//			- create an overflow page (if page.overflowPointerIndex == -1) using nextFreePage. update nextFreePage index and pageIndex. ✓

        // Seek to the appropriate position in the index file ✓
		// TODO: After inserting the record, write the modified page back to the index file. ✓
		//		 Remember to use the correct position (i.e., pageIndex) if you are writing out an overflow page! ✓
        
        int currentPageNum = pageNum;
        Page currentPage;
        bool inserted = false;
        
        while (!inserted) {
            // Seek to current page
            indexFile.seekg(currentPageNum * Page_SIZE, ios::beg);
            currentPage.read_from_data_file(indexFile);
            
            // Try to insert
            if (currentPage.insert_record_into_page(record)) {
                // Success - write back
                indexFile.seekp(currentPageNum * Page_SIZE, ios::beg);
                currentPage.write_into_data_file(indexFile);
                inserted = true;
            } else {
                // Page is full
                if (currentPage.overflowPointerIndex == -1) {
                    // Create new overflow page
                    int overflowPageNum = nextFreePage++;
                    Page overflowPage;
                    overflowPage.insert_record_into_page(record);
                    
                    // Update current page's overflow pointer
                    currentPage.overflowPointerIndex = overflowPageNum;
                    indexFile.seekp(currentPageNum * Page_SIZE, ios::beg);
                    currentPage.write_into_data_file(indexFile);
                    
                    // Write overflow page
                    indexFile.seekp(overflowPageNum * Page_SIZE, ios::beg);
                    overflowPage.write_into_data_file(indexFile);
                    inserted = true;
                } else {
                    // Move to overflow page
                    currentPageNum = currentPage.overflowPointerIndex;
                }
            }
        }
        
        // Close the index file
        indexFile.close();
    }

    // Function to search for a record by ID in a given page of the index file
    Record* searchRecordByIdInPage(int pageIndex, int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);
        if (!indexFile) return nullptr;
        
        int currentPageIndex = pageIndex;
        
        while (currentPageIndex != -1) {
            // Seek to the appropriate position in the index file
            indexFile.seekg(currentPageIndex * Page_SIZE, ios::beg);
            
            // Read the page from the index file
            Page page;
            if (!page.read_from_data_file(indexFile)) {
                break;
            }
            
            // TODO:
            // - Search for the record by ID in the page
            // - Check for overflow pages and report if record with given ID is not
            for (int i = 0; i < page.records.size(); i++) {
                if (page.records[i].id == id) {
                    indexFile.close();
                    // Create a dynamic copy
                    return new Record(page.records[i]);
                }
            }
            
            // Move to overflow page if exists
            currentPageIndex = page.overflowPointerIndex;
        }
        
        indexFile.close();
        return nullptr;
    }

public:
    HashIndex(string indexFileName) : nextFreePage(0), fileName(indexFileName) {
        PageDirectory.resize(256, -1);
        
        // Create/truncate the index file
        ofstream initFile(fileName, ios::binary | ios::trunc);
        initFile.close();
    }

    // Function to create hash index from Employee CSV file
    void createFromFile(string csvFileName) {
        // Read CSV file and add records to index
        // Open the CSV file for reading
        ifstream csvFile(csvFileName);
        if (!csvFile) {
            cerr << "Error: Cannot open CSV file: " << csvFileName << endl;
            return;
        }

        string line;
        int recordCount = 0;
        // Read each line from the CSV file
        while (getline(csvFile, line)) {
            if (line.empty()) continue;
            // Parse the line and create a Record object
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            if (fields.size() < 4) continue;
            
            Record record(fields);
            recordCount++;

            // TODO:
            // - Compute hash value for the record's ID using compute_hash_value() function.
            // - Get the page index from PageDirectory. If it's not in PageDirectory, define a new page using nextFreePage.
            // - Insert the record into the appropriate page in the index file using addRecordToIndex() function.        
            int hash_value = compute_hash_value(record.id);
            int pageNum = PageDirectory[hash_value];
            
            // If no page assigned for this hash value, create one
            if (pageNum == -1) {
                pageNum = nextFreePage++;
                PageDirectory[hash_value] = pageNum;
                
                // Initialize empty page in file
                fstream indexFile(fileName, ios::binary | ios::in | ios::out);
                indexFile.seekp(pageNum * Page_SIZE, ios::beg);
                Page emptyPage;
                emptyPage.write_into_data_file(indexFile);
                indexFile.close();
            }
            
            // Add record to index
            addRecordToIndex(pageNum, record);
        }
        
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // TODO:
        // - Compute hash value for the given ID using compute_hash_value() function
        // - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function

        int hash_value = compute_hash_value(id);
        int pageIndex = PageDirectory[hash_value];
        
        if (pageIndex == -1) {
            cout << "Employee with ID " << id << " not found." << endl;
            return;
        }
        
        Record* found = searchRecordByIdInPage(pageIndex, id);
        if (found != nullptr) {
            found->print();
            delete found;
        } else {
            cout << "Employee with ID " << id << " not found." << endl;
        }

        // Close the index file
        indexFile.close();
    }
};
