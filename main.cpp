#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <conio.h>
#include <regex>
#include <random>
#include <utility> //pair

using namespace std;

#define HASH_SIZE 1024
//global var
int totalPaket = 0;
int totalGudang = 0;
string fileTree = "./data/tree.txt";
string fileGraph = "./data/graph.txt";
string fileKota = "./data/kota.txt";
string filePaket = "./data/paket.txt";
string fileUser = "./data/user.txt";

//enum
enum MenuType{
    MAIN_MENU,
    ADMIN_MENU,
    USER_MENU
};

enum StatusType{
    TRANSIT_IN,
    CLIENT_IN,
    GUDANG,
    TRANSIT_OUT,
    OUT,
    HISTORY
};

//struct
struct DataClient{
    string nama;
    string telp;
    string alamat;
};

struct Paket{
    //data 
    string resi;
    string kategoriBarang;
    string waktuKirim;
    //metadata
    int statusType;
    string statusTime;
    //data pengirim & penerima 
    DataClient pengirim;
    DataClient penerima;
};

struct NodeHash{
    Paket* dataPtr {nullptr};
    NodeHash* next {nullptr};
};
struct PaketHashMap{
    NodeHash* head {nullptr};
}; PaketHashMap paketHash[HASH_SIZE]; 

//Q
struct NodeClientIn{
    Paket* data {nullptr};
    NodeClientIn* next {nullptr};
}; 
NodeClientIn* headClientIn = nullptr;
NodeClientIn* tailClientIn = nullptr;

struct NodeQueueOut{
    Paket* data {nullptr};
    NodeQueueOut* next {nullptr};
};
NodeQueueOut* headQueueOut = nullptr;
NodeQueueOut* tailQueueOut = nullptr;

struct NodeTransitIn{
    Paket* data {nullptr};
    NodeTransitIn* next {nullptr};
};
NodeTransitIn* headTransitIn = nullptr;
NodeTransitIn* tailTransitIn = nullptr;
struct NodeTransitOut{
    Paket* data {nullptr};
    NodeTransitOut* next {nullptr};
};
NodeTransitOut* headTransitOut = nullptr;
NodeTransitOut* tailTransitOut = nullptr;

struct NodeHistory{
    Paket* data {nullptr};
    NodeHistory* next{nullptr};
};
NodeHistory* headNodeHistory = nullptr; //top of the stack

struct NodeTree{
    string key;
    Paket* dataPtr = nullptr;
    vector<NodeTree*> child; //kalo manual bisa pake linked list tapi it's unecessarily complex to do CRUD, a lot of traversing just to do basic operation, with vector you could directly acess the child

    NodeTree(string k, Paket* ptr):
    key(k), dataPtr(ptr){};
}; NodeTree* rootTree = nullptr;


struct NodeGraph{
    string kota; //asal
    vector<pair<NodeGraph*, int>> adjList; //<tujuan, jarak>
};
vector<NodeGraph*> kotaGraph;

//prototype
void insertTreeNode(Paket &paket);
void removeTreeNode(Paket &paket);
void enqueuePaket(struct node); //universal enqueue
void dequeuePaket(struct node);
void pushHistory();
//func
string resiGen(string noTelp){
    string resi;
    string temp;
    int sumTelp = 0;
    //notelp because everyperson had different number
    for (char c:noTelp){
        sumTelp += c;
    }
    //pick timestamp
    string timestamp;
    time_t now = time(0);
    string timeSeconds = to_string(now);
    int sumTime=0;
    for(int i = 0; i < 8; i++){
        int temp = timeSeconds[i]-'0'; //since the character '0' has the ASCII code of 48 and 1 to 9 comes after it, it could be converted using this
        sumTime += temp;
    }
    timestamp = to_string(sumTime) + timeSeconds.substr(8,timeSeconds.length());
    //randomize last 3 digit
    int randNum = rand() % 1000;
    cout << randNum << endl;
    //add em up
    resi = "AJ" + to_string(sumTelp) + timestamp + to_string(sumTelp % 9) + to_string(randNum);
    //check length
    if (resi.length() > 13){
      resi = resi.substr(0,13);
    } else {
        resi.append(13 - resi.length(), '0'); //fill with 0 if it's < 13
    }
    return resi;
};

string getWaktuNow(){
    time_t now = time(0); //get current time in seconds based on unix time
    tm *tmNow = localtime(&now); //struct 
    char waktu[6]; // "HH:MM" + '\0'
    strftime(waktu, sizeof(waktu), "%H:%M", tmNow);

    return waktu;
}

//polynomial hashing
//https://github.com/trekhleb/javascript-algorithms/blob/master/src/algorithms/cryptography/polynomial-hash/README.md
int hashFunc(string resi){
    unsigned long sum = 0;
    int base = 67;
    for (char c:resi){
        sum += sum * base + c;
    }
    return sum % HASH_SIZE;
}


void initTreeDB(){
    ifstream baca(fileTree);
    if (baca.fail()) {
        cerr << "\nGagal membaca Database Tree";
        return exit(0);
    }
    string kategori;
    rootTree = new NodeTree("kategori paket", nullptr);
    while(getline(baca,kategori)){
        NodeTree* newKategori = new NodeTree(kategori, nullptr);
        rootTree->child.push_back(newKategori);
    }
    baca.close();
};

void showTree(NodeTree* root, int depth = 0){
    if(rootTree == nullptr){
        cout << "\n\nTree masih kosong.";
        return;
    }
    if(root == nullptr){
        return;
    }
    //fancy padding
    for (int i = 0; i < depth; i++) {
        cout << "| ";
    }
    if (depth > 0) {
        cout << " +-";
    }
    //DFS pre-order traversal
    cout << root->key << endl;
    //recursivly traverse to every child
    for(NodeTree* child:root->child) {
        showTree(child, depth + 1);
    }

}

//==
void quit() {
    //updatePaketDB();
    cout << "\n\nProgram End." << endl;
}

void init() {
    //initPaketDB();
}

int main() {
    srand(time(0));
    
    atexit(quit);
    // string no = "0812345678901";
    // string resi = resiGen(no);
    // cout << resi <<endl;
    // cout << hashFunc(resi);

    initTreeDB();
    showTree(rootTree);
    // init();
    return 0;
}
