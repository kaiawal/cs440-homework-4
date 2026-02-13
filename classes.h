#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
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
	
	// Function to get the size of the record
    int get_size() {
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
    Page() : overflowPointerIndex(-1) {}

    // Function to insert a record into the page
    bool insert_record_into_page(Record r) {
        int record_size = r.get_size();
        int slot_size = sizeof(int) * 2;
        if (cur_size + record_size + slot_size > 4096) { // Check if page size limit exceeded, considering slot directory size
            return false; // Cannot insert the record into this page
        } else {
            records.push_back(r);
            cur_size += record_size + slot_size;
			// TODO: update slot directory information
            return true;
        }
    }

    // Function to write the page to a binary output stream. You may use
    void write_into_data_file(ostream &out) const {
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;

        // Write records into page_data buffer
        for (const auto &record: records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // TODO:
        //  - Write slot_directory in reverse order into page_data buffer.
        //  - Write overflowPointerIndex into page_data buffer.
        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.

        // Write the page_data buffer to the output stream
        out.write(page_data, sizeof(page_data));
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream

        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            // TODO: Process data to fill the records, slot_directory, and overflowPointerIndex
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
    void addRecordToIndex(int pageIndex, Page &page, Record &record) {
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

        // Seek to the appropriate position in the index file
		// TODO: After inserting the record, write the modified page back to the index file. 
		//		 Remember to use the correct position (i.e., pageIndex) if you are writing out an overflow page!
        while (true) {
            indexFile.seekp(pageIndex * Page_SIZE, ios::beg); // sets cursor to beginning of page
            page.read_from_data_file(indexFile);

            // if page is full
            if (!page.insert_record_into_page(record)) {
                if (page.overflowPointerIndex = -1){ // no overflow page, so create one
                    int overflowIndex = nextFreePage++;
                    Page overflowPage;

                    // point the pointer to the overflow page
                    page.overflowPointerIndex = overflowIndex;

                    // write updated current page back
                    indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
                    page.write_into_data_file(indexFile);

                    // insert into new overflow page
                    overflowPage.insert_record_into_page(record);

                    // write overflow page
                    indexFile.seekp(overflowIndex * Page_SIZE, ios::beg);
                    overflowPage.write_into_data_file(indexFile);
                    break;

                } else { // there is an overflow page
                    pageIndex = page.overflowPointerIndex;
                    // no break!
                }
            } else {
                // write modified page to same position
                indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
                page.write_into_data_file(indexFile);
                break;
            }
        }
        // Close the index file
        indexFile.close();
    }

    // Function to search for a record by ID in a given page of the index file
    Record* searchRecordByIdInPage(int pageIndex, int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // Seek to the appropriate position in the index file
        indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

        // Read the page from the index file
        Page page;
        page.read_from_data_file(indexFile);

        // TODO: ✓
        //  - Search for the record by ID in the page
        //  - Check for overflow pages and report if record with given ID is not found
        
        // check current page for records
        for (int i = 0; i < page.records.size(); i++) {
            if (page.records[i].id == id) {
                return &page.records[i];
            }
        }

        if (page.overflowPointerIndex != -1) {
            searchRecordByIdInPage(page.overflowPointerIndex, id);
        } 

        return NULL;

    }

public:
    HashIndex(string indexFileName) : nextFreePage(0), fileName(indexFileName) {
        // PageDirectory.resize(256, -1);
    }

    // Function to create hash index from Employee CSV file
    void createFromFile(string csvFileName) {
        // Read CSV file and add records to index
        // Open the CSV file for reading
        ifstream csvFile(csvFileName);

        string line;
        // Read each line from the CSV file
        while (getline(csvFile, line)) {
            // Parse the line and create a Record object
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            Record record(fields);

            // TODO: ✓
            //   - Compute hash value for the record's ID using compute_hash_value() function. ✓
            //   - Get the page index from PageDirectory. If it's not in PageDirectory, define a new page using nextFreePage. ✓
            //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function. ✓
            int hash_id = compute_hash_value(record.id);
            // find page index
            int pageIndex = PageDirectory[hash_id];

            // if bucket isn't created yet
            if (pageIndex == -1) {
                // make a new page
                pageIndex = nextFreePage;
                PageDirectory[hash_id] = pageIndex;
                nextFreePage++;
            }

            // insert record into index
            Page page;
            addRecordToIndex(pageIndex, page, record);
        }

        // Close the CSV file
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // TODO: ✓
        //  - Compute hash value for the given ID using compute_hash_value() function
        //  - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function
        int hash_value = compute_hash_value(id);

        Record* found_id = searchRecordByIdInPage(hash_value, id);
        if (found_id != NULL) {
            found_id->print();
        }

        // Close the index file
        indexFile.close();
    }
};

