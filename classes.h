#ifndef CLASSES_H
#define CLASSES_H

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
    int id, manager_id;
    string bio, name;

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }
    
    // Default constructor for empty records
    Record() : id(0), manager_id(0) {}
    
    int get_size() const {
        return sizeof(id) + sizeof(manager_id) + sizeof(int) + name.size() + sizeof(int) + bio.size();
    }

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

    void print() const {
        cout << "\tID: " << id << "\n";
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};

class Page {
public:
    vector<Record> records;
    vector<pair<int, int>> slot_directory;
    int cur_size;
    int overflowPointerIndex;

    Page() : cur_size(sizeof(int)), overflowPointerIndex(-1) {}

    void clear() {
        records.clear();
        slot_directory.clear();
        cur_size = sizeof(int);
        overflowPointerIndex = -1;
    }

    bool insert_record_into_page(const Record &r) {
        int record_size = r.get_size();
        int slot_size = sizeof(int) * 2; // Each slot entry is two ints (offset and size)
        
        if (cur_size + record_size + slot_size > 4096) {
            return false;
        } else {
            // Calculate offset for new record (sum of sizes of existing records)
            int offset = 0;
            for (const auto &rec : records) {
                offset += rec.get_size();
            }
            
            records.push_back(r);
            slot_directory.push_back({offset, record_size});
            cur_size += record_size + slot_size;
            
            return true;
        }
    }

    void write_into_data_file(ostream &out) const {
        char page_data[4096] = {0};
        int offset = 0;

        // Write records sequentially
        for (const auto &record : records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

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

    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0};
        in.read(page_data, 4096);
        
        if (in.gcount() != 4096) {
            return false;
        }

        // Clear existing data
        clear();

        // Read overflow pointer from the end
        int current_pos = 4096 - sizeof(overflowPointerIndex);
        memcpy(&overflowPointerIndex, page_data + current_pos, sizeof(overflowPointerIndex));

        // Read slot count
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
};

class HashIndex {
private:
    const int Page_SIZE = 4096;
    vector<int> PageDirectory; // Maps hash value to page number
    int nextFreePage;
    string fileName;

    int compute_hash_value(int id) {
        return id % 256; // 2^8 = 256
    }

    void addRecordToIndex(int pageNum, Record &record) {
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);
        if (!indexFile) {
            cerr << "Error: Unable to open index file." << endl;
            return;
        }
        
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
        
        indexFile.close();
    }

    Record* searchRecordByIdInPage(int pageNum, int id, Page &page) {
        ifstream indexFile(fileName, ios::binary | ios::in);
        if (!indexFile) return nullptr;
        
        int currentPageNum = pageNum;
        
        while (currentPageNum != -1) {
            // Seek to page
            indexFile.seekg(currentPageNum * Page_SIZE, ios::beg);
            page.read_from_data_file(indexFile);
            
            // Search in current page
            for (auto &record : page.records) {
                if (record.id == id) {
                    indexFile.close();
                    return &record;
                }
            }
            
            // Move to overflow page
            currentPageNum = page.overflowPointerIndex;
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

    void createFromFile(string csvFileName) {
        ifstream csvFile(csvFileName);
        if (!csvFile) {
            cerr << "Error: Cannot open CSV file: " << csvFileName << endl;
            return;
        }

        string line;
        int recordCount = 0;
        
        while (getline(csvFile, line)) {
            if (line.empty()) continue;
            
            stringstream ss(line);
            string item;
            vector<string> fields;
            
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            
            if (fields.size() < 4) continue;
            
            Record record(fields);
            recordCount++;
            
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
        cout << "Created index with " << recordCount << " records in " << nextFreePage << " pages." << endl;
    }

    void findAndPrintEmployee(int id) {
        int hash_value = compute_hash_value(id);
        int pageNum = PageDirectory[hash_value];
        
        if (pageNum == -1) {
            cout << "Employee with ID " << id << " not found." << endl;
            return;
        }
        
        Page page;
        Record* found = searchRecordByIdInPage(pageNum, id, page);
        
        if (found != nullptr) {
            found->print();
        } else {
            cout << "Employee with ID " << id << " not found." << endl;
        }
    }
};

#endif