#include<iostream>
#include"table.h"

int main(){
    PageTable table;
    PageState *state = new PageState;
    state->st = SHARED;
    state->clientList.push_back(0);
    state->clientList.push_back(3);
    uint32_t val = table.formValue(state);
    printf("Val = %x\n", val);
    table.put(0, val);
    state->clientList.clear();
    state->clientList.push_back(10);
    val = table.formValue(state);
    printf("Val = %x\n", val);
    table.put(0, val);
    table.printTable();
    PageState *state2 = table.getState(val);
    state2->printState();
    return 0;
}
