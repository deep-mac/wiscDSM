#include"table.h"
#include <limits.h>

void PageState::printState(){
    auto it = clientList.begin();
    printf("st = %d, clientList = ", st);
    while(it != clientList.end()){
        printf("%d, ", *it);
        it++;
    }
    printf("\n");
}
void PageTable::printTable(){
    auto it = map.begin();
    while(it != map.end()){
        printf("Key = %d, Value = 0x%x\n", it->first, it->second);
        it++;
    }
}

int PageTable::put(int key, uint32_t value){
    map[key] = value;
    persist();
    return map[key];
}

void PageTable::persist(){
    std::ofstream f_out;
    f_out.open(fileName, std::fstream::trunc);
    for(auto it = map.begin(); it != map.end(); it++){
        std::string line = std::to_string(it->first) + "$" + std::to_string(it->second);
        f_out << line << std::endl;
    }
    f_out.close();
}

uint32_t PageTable::get(int key){
    if (map.find(key) == map.end()) {
        return INT_MAX;
    }
    return map[key];
}

PageState* PageTable::getState(uint32_t value){
    PageState *state = new PageState;
    if (value == 0 || value == INT_MAX){
        state->st = INV;
        return state;
    }
    if ((value >> 30)  == 0x1){
        state->st = SHARED;
    }
    else if ((value >> 30)  == 0x3){
        state->st = MODIFIED;
    }
    for (int i = 0; i < 30; i++){
        if ((value & (0x1 << i)) != 0){
            state->clientList.push_back(i);
        }
    }
    return state;
}

uint32_t PageTable::formValue (PageState* state){
    uint32_t val = 0;
    int setbit = 0x00;
    if (state->st == INV){
        val = 0;
    }
    else if (state->st == SHARED){
        val = (val & 0x3FFFFFFF) | 0x1 << 30;
    }
    else if (state->st == MODIFIED){
        val = (val & 0x3FFFFFFF) | (0x3 << 30);
    }
    for (int i = 0; i < state->clientList.size() ; i++){
        val = val | (0x1 << state->clientList[i]);
    }
    return val;
}
