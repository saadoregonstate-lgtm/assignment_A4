#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cmath>

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
    static const int PAGE_SIZE = 4096;
    char data[PAGE_SIZE];

    Page() {
        memset(data, 0, PAGE_SIZE);
        setOverflow(-1);
        setSlotCount(0);
    }

    int getOverflow() {
        return *(int*)(data + PAGE_SIZE - 4);
    }

    void setOverflow(int val) {
        *(int*)(data + PAGE_SIZE - 4) = val;
    }

    short getSlotCount() {
        return *(short*)(data + PAGE_SIZE - 6);
    }

    void setSlotCount(short val) {
        *(short*)(data + PAGE_SIZE - 6) = val;
    }

    bool insertRecord(const string &rec) {
        short slotCount = getSlotCount();
        int freePtr = 0;

        if (slotCount > 0) {
            int slotOffsetPos = PAGE_SIZE - 6 - sizeof(short)*2*slotCount;
            short lastOffset = *(short*)(data + slotOffsetPos);
            short lastLen = *(short*)(data + slotOffsetPos + sizeof(short));
            freePtr = lastOffset + lastLen;
        }

        int needed = rec.size() + sizeof(short)*2;

        int freeSpace = PAGE_SIZE - 6 - slotCount*sizeof(short)*2 - freePtr;

        if (needed > freeSpace)
            return false;

        memcpy(data + freePtr, rec.c_str(), rec.size());

        int slotPos = PAGE_SIZE - 6 - sizeof(short)*2*(slotCount+1);
        *(short*)(data + slotPos) = freePtr;
        *(short*)(data + slotPos + sizeof(short)) = rec.size();

        setSlotCount(slotCount + 1);
        return true;
    }
};

class LinearHashIndex {
private:
    const size_t maxCacheSize = 1; // Maximum number of pages in the buffer
    const int Page_SIZE = 4096; // Size of each page in bytes
    int n;  // The number of indexes (pages) being used
    int i;	// The number of least-significant-bits of h(id) to check. Will need to increase i once n > 2^i
    int numRecords;    // Records currently in index. Used to test whether to increase n
    string fileName;

    // Function to compute hash value for a given ID
    int compute_hash_value(int id) {
    int h = id % (1 << 12);  // id mod 2^12
    int mask = (1 << i) - 1;
    int bucket = h & mask;

    if (bucket >= n)
        bucket = h & ((1 << (i+1)) - 1);

    return bucket;
    }

    // Function to add a new record to an existing page in the index file
    void addRecordToIndex(int pageIndex, Page &page, Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }

        // TODO: 
        // Add record to the index in the correct page, creating a overflow page if necessary
        string serialized = record.serialize();

        if (!page.insertRecord(serialized)) {
            int overflowIndex = page.getOverflow();

            if (overflowIndex == -1) {
                indexFile.seekp(0, ios::end);
                overflowIndex = indexFile.tellp() / Page_SIZE;

                Page overflowPage;
                overflowPage.insertRecord(serialized);

                page.setOverflow(overflowIndex);

                indexFile.seekp(bucket * Page_SIZE, ios::beg);
                indexFile.write(page.data, Page_SIZE);

                indexFile.seekp(overflowIndex * Page_SIZE, ios::beg);
                indexFile.write(overflowPage.data, Page_SIZE);
            }
            else {
                indexFile.seekg(overflowIndex * Page_SIZE, ios::beg);
                Page overflowPage;
                overflowPage.read_from_data_file(indexFile);

                overflowPage.insertRecord(serialized);

                indexFile.seekp(overflowIndex * Page_SIZE, ios::beg);
                indexFile.write(overflowPage.data, Page_SIZE);
            }
        }
        else {
            indexFile.seekp(bucket * Page_SIZE, ios::beg);
            indexFile.write(page.data, Page_SIZE);
        }

        numRecords++;
        // Check and Take neccessary steps if capacity is reached:
        OverflowHandler();
		// increase n; increase i (if necessary); place records in the new bucket that may have been originally misplaced due to a bit flip



        // Seek to the appropriate position in the index file
        indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
        // TODO: Insert record to page and write data to file

        // Close the index file
        indexFile.close();
    }

    void OverflowHandler() {
        // TODO:
        // Calculate the average number of records per page

        double avg = (double)numRecords / n;

        if (avg > 0.7 * (Page_SIZE / 100)) {
            n++;

            if (n > (1 << i))
                i++;
        }

        // Take neccessary steps if capacity is reached
        // increase n; increase i (if necessary); redistribute records accordingly. place records in the new bucket that may have been originally misplaced due to a bit flip.
    }

    // Function to search for a record by ID in a given page of the index file
    void searchRecordByIdInPage(int pageIndex, int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // Seek to the appropriate position in the index file
        indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

        // Read the page from the index file
        Page page;
        page.read_from_data_file(indexFile);

        // TODO:
        //  - Search for the record by ID in the page
        //  - Check for overflow pages and report if record with given ID is not found
        short slotCount = page.getSlotCount();

    for (int s = 0; s < slotCount; s++) {
        int slotPos = PAGE_SIZE - 6 - sizeof(short)*2*(s+1);
        short offset = *(short*)(page.data + slotPos);
        short len = *(short*)(page.data + slotPos + sizeof(short));

        int recId = *(int*)(page.data + offset);

        if (recId == id) {
            cout << "Record Found!" << endl;
            return;
        }
    }

    int overflow = page.getOverflow();
    if (overflow != -1)
    searchRecordByIdInPage(overflow, id);
    }

public:
    LinearHashIndex(string indexFileName) : numRecords(0), fileName(indexFileName) {  
        n = 4; // Start with 4 buckets in index
        i = 2; // Need 2 bits to address 4 buckets
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

            // TODO:
            //   - Compute hash value for the record's ID using compute_hash_value() function.
            //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function.
            int bucket = compute_hash_value(record.id);

        Page page;
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            indexFile.open(fileName, ios::binary | ios::out);
            indexFile.close();
            indexFile.open(fileName, ios::binary | ios::in | ios::out);
        }

        indexFile.seekg(bucket * Page_SIZE, ios::beg);
        page.read_from_data_file(indexFile);

        addRecordToIndex(bucket, page, record);

        indexFile.close();

        }

        // Close the CSV file
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        // TODO:
        //  - Compute hash value for the given ID using compute_hash_value() function
        //  - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function

        // Close the index file
        indexFile.close();
    }
};

