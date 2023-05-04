#include<iostream>
#include<fstream>
#include<map>
#include<vector>
#include<sstream>

enum State {INV, SHARED, MODIFIED};

class PageState{
    public:
    int st = INV;
    std::vector<int> clientList;
    void printState();
};

class PageTable {
    public:
    std::map<int , uint32_t> map;
    std::string fileName = "table.txt";
    PageTable(){
        std::ifstream fh;
        fh.open(fileName);
        std::string line;
        while(getline(fh, line)) {
            std::stringstream ss(line);
            std::string key, value;
            getline(ss, key, '$');
            getline(ss, value, '$');
            map[stoi(key)] = stoi(value);
	    std::cout << key << " : "<< value << "\n";
        }
        fh.close();
    }
    int put(int key, uint32_t value);
    uint32_t get(int key);
    PageState* getState(uint32_t value);
    uint32_t formValue(PageState* state);
    void persist();
    void printTable();

};
