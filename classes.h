#ifndef CLASSES_H
#define CLASSES_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <cmath>

using namespace std;

class Record {
public:
    int id, manager_id;
    string name, bio;

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }

    int get_size() {
        return sizeof(id) + sizeof(manager_id)
             + sizeof(int) + name.size()
             + sizeof(int) + bio.size();
    }

    string serialize() const {
        ostringstream oss;

        oss.write((char*)&id, sizeof(id));
        oss.write((char*)&manager_id, sizeof(manager_id));

        int name_len = name.size();
        int bio_len = bio.size();

        oss.write((char*)&name_len, sizeof(name_len));
        oss.write(name.c_str(), name_len);

        oss.write((char*)&bio_len, sizeof(bio_len));
        oss.write(bio.c_str(), bio_len);

        return oss.str();
    }

    void print() {
        cout << "ID: " << id << endl;
        cout << "Name: " << name << endl;
        cout << "Bio: " << bio << endl;
        cout << "Manager ID: " << manager_id << endl;
    }
};

class Page {
public:
    vector<Record> records;
    vector<pair<int,int>> slot_directory;
    int overflowPointerIndex;
    int cur_size;

    Page() {
        overflowPointerIndex = -1;
        cur_size = sizeof(int);
    }

    bool insert_record_into_page(Record &r) {

        int record_size = r.get_size();
        int slot_size = sizeof(int) * 2;

        if (cur_size + record_size + slot_size > 4096)
            return false;

        int offset = 0;
        for (auto &rec : records)
            offset += rec.get_size();

        records.push_back(r);
        slot_directory.push_back({offset, record_size});
        cur_size += record_size + slot_size;

        return true;
    }

    void write_into_data_file(ostream &out) const {

        char page_data[4096] = {0};
        int offset = 0;

        for (const auto &rec : records) {
            string serialized = rec.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        int slot_pos = 4096 - sizeof(int);

        for (int i = slot_directory.size() - 1; i >= 0; i--) {
            slot_pos -= sizeof(int);
            memcpy(page_data + slot_pos,
                   &slot_directory[i].second, sizeof(int));

            slot_pos -= sizeof(int);
            memcpy(page_data + slot_pos,
                   &slot_directory[i].first, sizeof(int));
        }

        memcpy(page_data + 4096 - sizeof(int),
               &overflowPointerIndex, sizeof(int));

        out.write(page_data, 4096);
    }

    bool read_from_data_file(istream &in) {

        char page_data[4096] = {0};
        in.read(page_data, 4096);

        if (in.gcount() != 4096)
            return false;

        records.clear();
        slot_directory.clear();

        memcpy(&overflowPointerIndex,
               page_data + 4096 - sizeof(int),
               sizeof(int));

        int slot_pos = 4096 - sizeof(int);

        while (slot_pos > 0) {

            slot_pos -= sizeof(int);
            int length;
            memcpy(&length, page_data + slot_pos, sizeof(int));

            slot_pos -= sizeof(int);
            int offset;
            memcpy(&offset, page_data + slot_pos, sizeof(int));

            if (offset == 0 && length == 0)
                break;

            slot_directory.insert(slot_directory.begin(),
                                  {offset, length});
        }

        for (auto &slot : slot_directory) {

            int offset = slot.first;

            int id, manager_id;
            int name_len, bio_len;

            memcpy(&id, page_data + offset, sizeof(int));
            offset += sizeof(int);

            memcpy(&manager_id, page_data + offset, sizeof(int));
            offset += sizeof(int);

            memcpy(&name_len, page_data + offset, sizeof(int));
            offset += sizeof(int);

            string name(page_data + offset, name_len);
            offset += name_len;

            memcpy(&bio_len, page_data + offset, sizeof(int));
            offset += sizeof(int);

            string bio(page_data + offset, bio_len);

            vector<string> fields = {
                to_string(id),
                name,
                bio,
                to_string(manager_id)
            };

            records.push_back(Record(fields));
        }

        return true;
    }
};

class LinearHashIndex {

private:
    const int Page_SIZE = 4096;
    const int MAIN_PAGE_LIMIT = 5000;

    int n;
    int i;
    int numRecords;
    int nextOverflowPage;

    string fileName;

    int compute_hash_value(int id) {
        return id % (1 << 12);
    }

    int get_bucket(int id) {
        int hash = compute_hash_value(id);
        int bucket = hash & ((1 << i) - 1);

        if (bucket >= n)
            bucket = hash & ((1 << (i - 1)) - 1);

        return bucket;
    }

    int allocateOverflowPage() {
        return nextOverflowPage++;
    }

    void OverflowHandler() {
    // Compute average records per main page
    double avgRecordsPerPage = (double)numRecords / n;

    // Approximate threshold: 70% of page capacity
    const int PAGE_CAPACITY_ESTIMATE = 4096 / ((4+4) + (200+500)/2); // rough average record size
    const double THRESHOLD = 0.7 * PAGE_CAPACITY_ESTIMATE;

    if (avgRecordsPerPage <= THRESHOLD)
        return; // No split needed

    // Split the next bucket (pointed by n)
    int splitBucket = n;

    // Prepare to redistribute records
    fstream indexFile(fileName, ios::binary | ios::in | ios::out);

    // Read the bucket page to split
    Page oldPage;
    indexFile.seekg(splitBucket * Page_SIZE);
    oldPage.read_from_data_file(indexFile);

    // Clear old page and reset overflow
    oldPage.records.clear();
    oldPage.slot_directory.clear();
    oldPage.overflowPointerIndex = -1;

    // Increment n and i if needed
    n++;
    if (n > (1 << i)) i++;

    // We need to redistribute all records from old bucket + overflow chain
    vector<Record> allRecords;

    int currentPageIndex = splitBucket;
    while (currentPageIndex != -1) {
        Page page;
        indexFile.seekg(currentPageIndex * Page_SIZE);
        page.read_from_data_file(indexFile);

        for (auto &r : page.records)
            allRecords.push_back(r);

        currentPageIndex = page.overflowPointerIndex;
    }

    // Clear old bucket
    Page clearedPage;
    indexFile.seekp(splitBucket * Page_SIZE);
    clearedPage.write_into_data_file(indexFile);

    // Redistribute records
    for (auto &r : allRecords) {
        int newBucket = get_bucket(r.id);

        Page page;
        indexFile.seekg(newBucket * Page_SIZE);
        page.read_from_data_file(indexFile);

        if (!page.insert_record_into_page(r)) {
            // Allocate overflow page
            int overflowPageIndex = allocateOverflowPage();
            page.overflowPointerIndex = overflowPageIndex;

            indexFile.seekp(newBucket * Page_SIZE);
            page.write_into_data_file(indexFile);

            // Write record into new overflow page
            Page overflowPage;
            overflowPage.insert_record_into_page(r);
            indexFile.seekp(overflowPageIndex * Page_SIZE);
            overflowPage.write_into_data_file(indexFile);
        } else {
            indexFile.seekp(newBucket * Page_SIZE);
            page.write_into_data_file(indexFile);
        }
    }

    indexFile.close();
}

public:
    LinearHashIndex(string fname) {
        fileName = fname;
        n = 4;
        i = 2;
        numRecords = 0;
        nextOverflowPage = MAIN_PAGE_LIMIT;
    }

    void createFromFile(string csvFileName) {

        ofstream initFile(fileName, ios::binary);
        Page emptyPage;

        for (int j = 0; j < MAIN_PAGE_LIMIT; j++)
            emptyPage.write_into_data_file(initFile);

        initFile.close();

        ifstream csvFile(csvFileName);
        string line;

        while (getline(csvFile, line)) {

            stringstream ss(line);
            string item;
            vector<string> fields;

            while (getline(ss, item, ','))
                fields.push_back(item);

            Record record(fields);

            int bucket = get_bucket(record.id);

            fstream indexFile(fileName,
                              ios::binary | ios::in | ios::out);

            indexFile.seekg(bucket * Page_SIZE);

            Page page;
            page.read_from_data_file(indexFile);

            if (!page.insert_record_into_page(record)) {

                int overflowPageIndex = allocateOverflowPage();
                page.overflowPointerIndex = overflowPageIndex;

                indexFile.seekp(bucket * Page_SIZE);
                page.write_into_data_file(indexFile);

                Page overflowPage;
                overflowPage.insert_record_into_page(record);

                indexFile.seekp(overflowPageIndex * Page_SIZE);
                overflowPage.write_into_data_file(indexFile);
            }
            else {
                indexFile.seekp(bucket * Page_SIZE);
                page.write_into_data_file(indexFile);
            }

            indexFile.close();
            numRecords++;

            OverflowHandler();
        }

        csvFile.close();
    }

    void findAndPrintEmployee(int id) {

        int bucket = get_bucket(id);

        ifstream indexFile(fileName, ios::binary);

        int currentPage = bucket;

        while (currentPage != -1) {

            indexFile.seekg(currentPage * Page_SIZE);

            Page page;
            page.read_from_data_file(indexFile);

            for (auto &rec : page.records)
                if (rec.id == id)
                    rec.print();

            currentPage = page.overflowPointerIndex;
        }

        indexFile.close();
    }
};

#endif