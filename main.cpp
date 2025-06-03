#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm> //remove func
#include <vector>
#include <ctime>
#include <conio.h>
#include <regex>
#include <random>
#include <cmath> //for rounding float to int
//dijkstra stuff
#include <queue>
#include <unordered_map>
#include <limits>

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
    TRANSIT_ACC,
    CLIENT_OUT,
    DELIVERED,
    RETURNED
};

enum InputType{
    NAMA, 
    TELP, 
    RESI
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
    string berat;
    //metadata
    int statusType;
    string statusTime; //last time status got changed
    //data pengirim & penerima 
    DataClient pengirim;
    DataClient penerima;

    Paket(string r = "", string k = "", string w = "", string b = "", int s = 0, string st = "", DataClient p = {}, DataClient pe = {}):
    resi(r), kategoriBarang(k), waktuKirim(w), berat(b), statusType(s), statusTime(st), pengirim(p), penerima(pe){}
};

struct NodeHash{
    Paket* dataPtr {nullptr};
    NodeHash* next {nullptr};

    NodeHash(Paket* d):
    dataPtr(d){}
};
struct PaketHashMap{
    NodeHash* head {nullptr};
}; PaketHashMap paketHash[HASH_SIZE]; 

//Q
struct NodeQueue{
    Paket* data {nullptr};
    NodeQueue* next {nullptr};

    NodeQueue(Paket* d):
    data(d){}
}; 
NodeQueue* headClientIn = nullptr;
NodeQueue* tailClientIn = nullptr;

NodeQueue* headClientOut = nullptr;
NodeQueue* tailClientOut = nullptr;

NodeQueue* headTransitIn = nullptr;
NodeQueue* tailTransitIn = nullptr;

NodeQueue* headTransitOut = nullptr;
NodeQueue* tailTransitOut = nullptr;

//stack, history sampai ke pelanggan
struct NodeHistory{
    Paket* data {nullptr};
    NodeHistory* next{nullptr};

    NodeHistory(Paket* d):
    data(d){};
};
NodeHistory* headNodeHistory = nullptr; //top of the stack

struct NodeTree{
    string key;
    Paket* dataPtr = nullptr;
    vector<NodeTree*> child; //kalo manual bisa pake linked list tapi it's unecessarily complex to do CRUD, a lot of traversing just to do basic operation, with vector you could directly acess the child

    NodeTree(string k, Paket* ptr):
    key(k), dataPtr(ptr){};
}; NodeTree* rootTree = nullptr;


//prototype
struct Edge;
struct NodeGraph{
    string kota; //asal
    vector<Edge> adjList; //<tujuan, jarak>
};
vector<NodeGraph*> kotaGraph;

struct Edge {
    NodeGraph* kotaTujuan;
    int jarak; //KM;
};

//prototype
void enqueuePaket(Paket* data, NodeQueue*& head, NodeQueue*& tail); //universal enqueue
void dequeuePaket(NodeQueue*& head, NodeQueue*& tail);
void pushHistory(Paket* data);
void popHistory(Paket*& data);
void addToTree(Paket* data);
void deleteFromTree(Paket* paket);
void addToHash(Paket* paket);
void deleteFromHash(Paket*& paket);
void jarakGraph(); //graph shortest path from a to b
int getJarak(const string& asal, const string& tujuan);
void updatePaketDB();
//? void saveHash();

//UTILS 
//resi generator
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
    // cout << randNum << endl;
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

//helper waktu HH:MM
string getWaktuNow(){
    time_t now = time(0); //get current time in seconds based on unix time
    tm *tmNow = localtime(&now); //struct 
    char waktu[6]; // "HH:MM" + '\0'
    strftime(waktu, sizeof(waktu), "%H:%M", tmNow);

    return waktu;
}
//helper tanggal YYYY:MM:DD
string getDateNow(){
    time_t now = time(0); //get current time in seconds based on unix time
    tm *tmNow = localtime(&now); //struct 
    char waktu[11]; // "YYYY-MM-DD" + '\0'
    strftime(waktu, sizeof(waktu), "%Y-%m-%d", tmNow);

    return waktu;
}

//utils kalkulasi ongkir
int getOngkir(float berat, string kotaAsal, string kotaTujuan){
    float jarak = getJarak(kotaAsal, kotaTujuan);
    double tarifPerKg;
    if (jarak <= 50) tarifPerKg = 9000;
    else if (jarak <= 200) tarifPerKg = 12000;
    else if (jarak <= 500) tarifPerKg = 15000;
    else tarifPerKg = 20000;
    
    int ongkir = ceil(berat) * tarifPerKg;
    return ongkir;
}

char inputHandler(){
    char p;
    p = _getch();
    if (p == 3){ //CTRL+C (ASCII = 3) will quit the program
        exit(0);
    }
    return p;
}


bool regexValidator(InputType inpuType, string inputStr){
    switch (inpuType)
    {
    case NAMA:
        return regex_match(inputStr, regex("^[a-zA-Z ]+$"));
    case TELP:
        return regex_match(inputStr, regex("^[0-9]{10,12}$"));
    case RESI:
        return regex_match(inputStr, regex("^AJ[0-9]{11}$")); // AJ + 11 digits, total 13 chars
    default:
        return false;
    }
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

void addToHash(Paket* data){
    int hashIndex = hashFunc(data->resi);
    NodeHash* temp = paketHash[hashIndex].head;
    while(temp->next != nullptr){
        temp = temp->next;
    }

    NodeHash* newNode = new NodeHash(data);
    temp->next = newNode;
    // cout << "\n[Paket berhasil ditambahkan ke hashmap]";
    return;
};

void deleteFromHash(Paket*& data){
    if(data == nullptr){
        cout << "\n\n[Paket tidak ditemukan]";
        return;
    }

    int hashIndex = hashFunc(data->resi);

    NodeHash* delNode = paketHash[hashIndex].head;
    NodeHash* prev = nullptr;
    
    while(delNode != nullptr && delNode->dataPtr->resi != data->resi){
        prev = delNode;
        delNode = delNode->next;
    }

     if(delNode == nullptr){
        cout << "\n\n[Paket tidak ditemukan]";
        return;
    }

    if(delNode == paketHash[hashIndex].head){
        paketHash[hashIndex].head = delNode->next;
    } else {
        prev->next = delNode->next;
    }
    delete delNode->dataPtr; //delete the Paket data
    delete delNode;
    cout << "\n[Paket berhasil dihapus dari hash]";
    return;
}

Paket* findInHash(string resi){
    int hashIndex = hashFunc(resi);
    NodeHash* helper = paketHash[hashIndex].head;
    
    while(helper != nullptr && helper->dataPtr->resi != resi){
        helper = helper->next;
    }
    
    if(helper == nullptr){
        cout << "\n\n[Paket tidak ditemukan]";
        return nullptr;
    }
    //found
    cout << "\n[Paket ditemukan]";
    return helper->dataPtr;
};

//Queue function, pass pointer by reference to update the global variable
void enqueuePaket(Paket* data, NodeQueue*& head, NodeQueue*& tail){
    if(data == nullptr){
        cout << "\n\nData Paket Kosong!";
        system("pause");
        return;
    }
    
    NodeQueue* newNode = new NodeQueue(data);
    if (head == nullptr) {
        head = tail = newNode;
    } else {
        tail->next = newNode;
        tail = newNode;
    }

    return;
};

//helper untuk enqueue paket sesuai statusnya
void enqueueHelper(Paket* data){
    switch (data->statusType)
    {
    case CLIENT_IN:
        enqueuePaket(data, headClientIn, tailClientIn);
        break;
    case TRANSIT_IN:
        enqueuePaket(data, headTransitIn, tailTransitIn);
        break;
    case GUDANG:
        if (data->penerima.alamat != "SURABAYA"){
            enqueuePaket(data, headTransitOut, tailTransitOut); //paket keluar gudang
            data->statusType = TRANSIT_OUT; //paket sudah disortir
        } else {
            enqueuePaket(data, headClientOut, tailClientOut); //paket diantar ke penerima
            data->statusType = CLIENT_OUT; //paket dalam proses pengantaran
        }
    default:
        break;
    }

}

void dequeuePaket(NodeQueue*& head, NodeQueue*& tail){
    if(head == nullptr || head->data == nullptr){
        cout << "\nGagal menghapus, data antrian kosong!";
        system("pause");
        return;
    }

    NodeQueue* delNode = head;
    if(head != tail){
        head = head->next;
    } else {
        head = tail = nullptr;
    }

    delete delNode;
    return;
};

//!WIP
//helper untuk dequeue paket sesuai statusnya
void dequeueHelper(Paket*& data){
    data->statusTime = getWaktuNow();
    switch (data->statusType)
    {
    case CLIENT_IN:
        addToTree(data); //paket masuk gudang
        dequeuePaket(headClientIn, tailClientIn); //paket dihapus dari antrian
        data->statusType = GUDANG;
        break;
    case TRANSIT_IN:
        addToTree(data); //paket masuk gudang
        dequeuePaket(headTransitIn, tailTransitIn); //paket dihapus dari antrian
        data->statusType = GUDANG;
        break;
    case CLIENT_OUT:
        pushHistory(data); //paket diantar ke penerima
        dequeuePaket(headClientOut, tailClientOut); //paket dihapus dari antrian
        data->statusType = DELIVERED;
        break;
    case TRANSIT_OUT:
        dequeuePaket(headTransitOut, tailTransitOut); //paket dihapus dari antrian
        data->statusType = TRANSIT_ACC; //paket sudah disortir
        break;
    default:
        break;
    }
};

//stack operation
void pushHistory(Paket* data){
    NodeHistory* newNode = new NodeHistory(data);
    newNode->next = headNodeHistory; //it point at nullptr if stack is empty, so no prob here
    headNodeHistory = newNode;

    headNodeHistory->data->statusType = DELIVERED;
    return;
};

void popHistory(Paket*& data){
    NodeHistory* delNode = headNodeHistory;

    headNodeHistory = delNode->next;

    delNode->data->statusType = RETURNED;
    delNode->data->statusTime = getWaktuNow();
    DataClient temp = delNode->data->pengirim;
    delNode->data->pengirim = delNode->data->penerima; //pengirim jadi penerima
    delNode->data->penerima = temp;

    enqueuePaket(data, headClientIn, tailClientIn); //paket dikembalikan
    delete delNode;
    return;
}

//clear all history
void clearHistory(){
    if (headNodeHistory == nullptr) {
        cout << "\n\nHistory kosong!";
        system("pause");
        return;
    }
    NodeHistory* helper = headNodeHistory;
    while (helper != nullptr) {
        NodeHistory* temp = helper;
        helper = helper->next;
        deleteFromHash(temp->data); //delete from hash
        delete temp; //delete node
    }
    headNodeHistory = nullptr;
    cout << "\n[History telah dibersihkan.]";
};

//traverse and print list
void showQueue(NodeQueue* head){
    if (head == nullptr) {
        cout << "\n\nData antrian kosong!";
        system("pause");
        return;
    }

    NodeQueue* helper = head;
    int num = 0;
    while(helper != nullptr) {
        cout << "[" << num << "]" <<endl;
        cout << "Resi" << helper->data->resi << endl;
        cout << "Pengirim" << helper->data->pengirim.nama << endl;
        cout << "Tujuan" << helper->data->penerima.alamat << endl;
        //? status :
        helper = helper->next;
        num++;
    }
};

void showHistory(NodeHistory* head){
    if (head == nullptr) {
        cout << "\n\nData History kosong!";
        system("pause");
        return;
    }

    NodeHistory* helper = head;
    int num = 0;
    while(helper != nullptr) {
        cout << "[" << num << "]" <<endl;
        cout << "Resi" << helper->data->resi << endl;
        cout << "Pengirim" << helper->data->pengirim.nama << endl;
        cout << "Tujuan" << helper->data->penerima.alamat << endl;
        //? status :
        helper = helper->next;
    }
};

//Tree function
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

void addToTree(Paket* data){
    bool found = false;
    if(data == nullptr){
        cout << "\n\nData masih kosong.";
        return;
    }
    NodeTree* newChild = new NodeTree(data->resi, data);
    //hwo
    for(NodeTree* child : rootTree->child){
        if(child->key == data->kategoriBarang){
            child->child.push_back(newChild);
            found = true;
            break;
        }
    }

    if (!found){
        cout << "\n\nKategori paket [" << data->kategoriBarang << "] tidak ditemukan";
    }
    return;
};

void deleteFromTree(Paket* data){
    bool found = false;
    if(rootTree == nullptr){
        cout << "\n\nTree masih kosong.";
        return;
    }

    for(NodeTree* kategoriNode : rootTree->child){
        if(kategoriNode->key == data->kategoriBarang){
            for(auto it = kategoriNode->child.begin(); it != kategoriNode->child.end(); ++it){
                if((*it)->key == data->resi){
                    delete *it;
                    kategoriNode->child.erase(it);
                    found = true;
                    cout << "\nPaket berhasil dihapus dari Gudang";
                    break;
                }
                
            }
            break;
        }
    }
    if(!found){
        cout << "\n\nPaket tidak ditemukan";
    }
    return;
};

void enqueueFromTree(){
    if(rootTree == nullptr){
        cout << "\n\nTree masih kosong.";
        return;
    }

    for(NodeTree* kategoriNode : rootTree->child){
        for(NodeTree* paketNode : kategoriNode->child){
            enqueueHelper(paketNode->dataPtr);
            //remove from tree
            deleteFromTree(paketNode->dataPtr);
        }
    }
};

//Graph
//Helper to find city node in kotaGraph
NodeGraph* findKotaNode(string nama){
    for(auto node: kotaGraph) {
        if(node->kota == nama){
            return node;
        }
    }
    return nullptr; //kalo ga ketemu
}

void showGraph() {
    cout << "\n=== Daftar Kota dan Koneksinya ===\n";
    for (auto node : kotaGraph) {
        cout << node->kota << " terhubung ke:\n";
        for (const auto& edge : node->adjList) {
            cout << "  - " << edge.kotaTujuan->kota << " (" << edge.jarak << " km)\n";
        }
        cout << endl;
    }
}
//Dijkstra Shortest Path
//!I DON'T UNDESTAND 
int getJarak(const string& asal, const string& tujuan) {
    NodeGraph* start = findKotaNode(asal);
    NodeGraph* end = findKotaNode(tujuan);
    if (!start || !end) {
        cout << "\nKota asal atau tujuan tidak ditemukan.\n";
        return 0;
    }

    unordered_map<NodeGraph*, int> dist;
    unordered_map<NodeGraph*, NodeGraph*> prev;
    for (auto node : kotaGraph) {
        dist[node] = numeric_limits<int>::max();
        prev[node] = nullptr;
    }
    dist[start] = 0;

    // Min-heap: pair<distance, NodeGraph*>
    priority_queue<pair<int, NodeGraph*>, vector<pair<int, NodeGraph*>>, greater<>> pq;
    pq.push({0, start});

    while (!pq.empty()) {
        auto [currentDist, current] = pq.top();
        pq.pop();

        if (current == end) break;

        for (const auto& edge : current->adjList) {
            int newDist = currentDist + edge.jarak;
            if (newDist < dist[edge.kotaTujuan]) {
                dist[edge.kotaTujuan] = newDist;
                prev[edge.kotaTujuan] = current;
                pq.push({newDist, edge.kotaTujuan});
            }
        }
    }

    if (dist[end] == numeric_limits<int>::max()) {
        cout << "\nTidak ada jalur dari " << asal << " ke " << tujuan << ".\n";
        return 0;
    }

    // Print path
    vector<string> path;
    for (NodeGraph* at = end; at != nullptr; at = prev[at]) {
        path.push_back(at->kota);
    }
    reverse(path.begin(), path.end());

    cout << "\nJalur terpendek dari " << asal << " ke " << tujuan << ":\n";
    for (size_t i = 0; i < path.size(); ++i) {
        cout << path[i];
        if (i + 1 < path.size()) cout << " -> ";
    }
    cout << "\nJarak total: " << dist[end] << " km\n";

    return dist[end];
}

//DB
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


void initGraphDB(){
    ifstream bacaKota{fileKota};
    ifstream bacaJarak(fileGraph);

    if (bacaKota.fail() || bacaJarak.fail()){
        cerr << "\nGagal membaca Database Graph";
        return exit(0);
    }

    string kota;
    while(getline(bacaKota, kota)){
        NodeGraph* newKota = new NodeGraph;
        newKota->kota = kota;
        kotaGraph.push_back(newKota);
    }

    string line;
    while(getline(bacaJarak, line)){
        stringstream ss(line);
        string asal, tujuan, jarakStr;
        getline(ss, asal, '|');
        getline(ss, tujuan, '|');
        getline(ss, jarakStr, '|');
        int jarak = stoi(jarakStr);

        NodeGraph* nodeAsal = findKotaNode(asal); 
        NodeGraph* nodeTujuan = findKotaNode(tujuan);
        
        //undirected, so add to both
        if(nodeAsal && nodeTujuan){
            nodeAsal->adjList.push_back({nodeTujuan, jarak});
            nodeTujuan->adjList.push_back({nodeAsal, jarak});
        } else {
            cout << "\n\n[Kota tidak ditemukan]";
        }
    }

    bacaKota.close();
    bacaJarak.close();
};

void initPaketDB(){
    ifstream baca(filePaket);
    if (baca.fail()){
        cerr << "\n\nGagal membaca Database Paket";
        return exit(0);
    }

    string line;
    while(getline(baca, line)) {
        stringstream ss;
        ss.str(line);
        Paket* paket = new Paket;
        string statusStr;

        // Format file: resi|kategoriBarang|waktuKirim|berat|statusType|statusTime|pengirim_nama|pengirim_telp|pengirim_alamat|penerima_nama|penerima_telp|penerima_alamat
        getline(ss, paket->resi, '|');
        getline(ss, paket->kategoriBarang, '|');
        getline(ss, paket->waktuKirim, '|');
        getline(ss, paket->berat, '|');
        getline(ss, statusStr, '|');
        paket->statusType = stoi(statusStr);
        getline(ss, paket->statusTime, '|');
        getline(ss, paket->pengirim.nama, '|');
        getline(ss, paket->pengirim.telp, '|');
        getline(ss, paket->pengirim.alamat, '|');
        getline(ss, paket->penerima.nama, '|');
        getline(ss, paket->penerima.telp, '|');
        getline(ss, paket->penerima.alamat, '|');

        // Add to hash map
        addToHash(paket);

        // Add to corresponding data structure
        switch (paket->statusType)
        {
        case TRANSIT_IN:
            enqueuePaket(paket, headTransitIn, tailTransitIn);
            break;
        case CLIENT_IN:
            enqueuePaket(paket, headClientIn, tailClientIn);
            break;
        case GUDANG:
            addToTree(paket);
            break;
        case TRANSIT_OUT:
            enqueuePaket(paket, headTransitOut, tailTransitOut);
            break;
        case CLIENT_OUT:
            enqueuePaket(paket, headClientOut, tailClientOut);
            break;
        case DELIVERED:
            pushHistory(paket);
            break;
        case RETURNED:
            addToTree(paket);
            break;
        default:
            break;
        }
    }

};

void savePaketDB(){
    //rewrite paket.txt, save from hash
    ofstream tulis(filePaket);
    if(tulis.fail()){
        cerr << "Gagal membuka file Paket";
        return exit(0);
    }
    for (int i = 0; i < HASH_SIZE; ++i) {
        NodeHash* node = paketHash[i].head;
        while (node != nullptr) {
            Paket* p = node->dataPtr;
            // Format: resi|kategoriBarang|waktuKirim|berat|statusType|statusTime|pengirim_nama|pengirim_telp|pengirim_alamat|penerima_nama|penerima_telp|penerima_alamat
            if(p->statusType != TRANSIT_ACC) { //paket yang sudah keluar dari gudang tidak disave
            tulis << p->resi << '|'
                << p->kategoriBarang << '|'
                << p->waktuKirim << '|'
                << p->berat << '|'
                << p->statusType << '|'
                << p->statusTime << '|'
                << p->pengirim.nama << '|'
                << p->pengirim.telp << '|'
                << p->pengirim.alamat << '|'
                << p->penerima.nama << '|'
                << p->penerima.telp << '|'
                << p->penerima.alamat << '\n';
            }
            node = node->next;
        }
    }
    tulis.close();
    cout << "\n[Data paket berhasil disimpan ke file]";
};

//CLI 

//helper getStatus
string getStatus(Paket* data){
    string statusPaket = "";
    string statusStr;
    if(!data){
        cout << "\n\n[Data kosong!]";
        return statusPaket;
    }
    switch (data->statusType)
    {
    case TRANSIT_IN:
        statusStr = "Paket tiba di Surabaya [Rungkut DC]";
        break;
    case CLIENT_IN:
        statusStr = "Menunggu konfirmasi";
        break;
    case GUDANG:
        statusStr = "Paket di gudang Surabaya [Rungkut DC]";
        break;
    case TRANSIT_OUT:
        statusStr = "Paket telah disortir";
        break;
    case TRANSIT_ACC:
        statusStr = "Paket keluar dari Surabaya [Rungkut DC]";
        break;
    case CLIENT_OUT:
        statusStr = "Paket diantar ke alamat tujuan";
        break;
    case DELIVERED:
        statusStr = "Paket telah diterima";
        break;
    case RETURNED:
        statusStr = "Paket dikembalikan";
        break;
    default:
        break;
    }

    statusPaket = "[" + getDateNow() + " <" + getWaktuNow(); + ">]";
    statusPaket = statusPaket + " - " + statusStr;
    return statusPaket;
}

void menuCekResi(){
    system("cls");
    string resi;
    cout << "===== Cek Resi =====\n";
    cout << "Resi : ";
    cin >> resi;
    //regex input invalid return
    if(!regexValidator(RESI, resi)){
        cout << "\n\n[Resi tidak valid!]";
        system("pause");
        return;
    }

    Paket* data;
    data = findInHash(resi);
    if(data){
        cout << "\n===== Detail Paket =====\n";
        cout << "Resi           : " << data->resi << endl;
        cout << "Kategori       : " << data->kategoriBarang << endl;
        cout << "Berat          : " << data->berat << " Kg" << endl;
        cout << "Waktu Kirim    : " << data->waktuKirim << endl;
        cout << "Pengirim       : " << data->pengirim.nama << endl;
        cout << "Telp Pengirim  : " << data->pengirim.telp << endl;
        cout << "Alamat Pengirim: " << data->pengirim.alamat << endl;
        cout << "Penerima       : " << data->penerima.nama << endl;
        cout << "Telp Penerima  : " << data->penerima.telp << endl;
        cout << "Alamat Tujuan  : " << data->penerima.alamat << endl;
        cout << "Status         : " << getStatus(data) << endl;
    }
    system("pause");
    return;
}

void menuKirimPaket(){
    string kategoriBarang, beratStr, alamatKirim, alamatTujuan;
    int ongkir;
    float berat;
    while(true){
        int pilihan = 0;
        system("cls");
        cout << "===== Kirim Paket =====";
        //data umum
        //kategori barang
        cout << "\nKategori Barang Tersedia:\n";
        for (size_t i = 0; i < rootTree->child.size(); ++i) {
            cout << "[" << i+1 << "] " << rootTree->child[i]->key << endl;
        }
        cout << "\nKategori Barang: ";
        cin.ignore();
        cin >> pilihan;
        if (pilihan < 1 || pilihan > rootTree->child.size()) {
            cout << "\n\n[Kategori tidak valid!]";
            system("pause");
            continue; //invalid input, continue to loop
        }
        kategoriBarang = rootTree->child[pilihan-1]->key;
        //berat
        cout << "Berat (Kg): ";
        cin >> berat;
        //validasi berat
        if (berat <= 0 || berat > 100) { //max 100KG
            cout << "\n\n[Berat tidak valid!]";
            system("pause");
            continue;;
        }
        beratStr = to_string(berat);
        //list kota        
        cout << "\nDaftar Kota Tersedia:\n";
        for (size_t i = 0; i < kotaGraph.size(); ++i) {
            cout << "[" << i+1 << "] " << kotaGraph[i]->kota << endl;
        }
        
        cout << "Alamat Pengirim: ";
        cin >> pilihan;
        if (pilihan < 1 || pilihan > kotaGraph.size()) {
            cout << "\n\n[Kota tidak valid!]";
            system("pause");
            continue; //invalid input, continue to loop
        }
        alamatKirim = kotaGraph[pilihan-1]->kota;
        cout << "Alamat Penerima: ";
        cin >> pilihan;
        cin.ignore(); 
        if (pilihan < 1 || pilihan > kotaGraph.size()) {
            cout << "\n\n[Kota tidak valid!]";
            system("pause");
            continue; //invalid input, continue to loop
        }
        alamatTujuan = kotaGraph[pilihan-1]->kota;
    }
    //cek ongkir
    ongkir = getOngkir(berat, alamatKirim, alamatTujuan);
    cout << "Biaya Ongkir : Rp. " << ongkir;
    char choice;
    cout << "\n\nKonfirmasi? (Y/N): ";

    choice = inputHandler();
    if (choice != 'Y' && choice != 'y') {
        cout << "\n\n[Paket tidak jadi dikirim]";
        system("pause");
        return;
    }
    //data pengirim
    DataClient pengirim;
    
    while(true){
        system("cls");
        cout << "\n\n===== Data Pengirim =====\n";
        cout << "Nama Pengirim: ";
        getline(cin, pengirim.nama);
        cout << "No. Telp Pengirim: ";
        getline(cin, pengirim.telp);
        pengirim.alamat = alamatKirim; //pakai alamat kirim

        //regex invalid continue else break
        if(!regexValidator(NAMA, pengirim.nama) || !regexValidator(TELP, pengirim.telp)){
            cout << "\n\n[Input tidak valid!]";
            system("pause");
            continue;
        } else {
            break; 
        }
    }
    //data penerima
    DataClient penerima;
    while(true){
        system("cls");
        //data penerima
        cout << "\n\n===== Data Penerima =====\n";
        cout << "Nama Penerima: "; 
        getline(cin, penerima.nama);
        cout << "No. Telp Penerima: ";
        getline(cin, penerima.telp);
        penerima.alamat = alamatTujuan; //pakai alamat tujuan

        //regex invalid continue else break
        if(!regexValidator(NAMA, penerima.nama) || !regexValidator(TELP, penerima.telp)){
            cout << "\n\n[Input tidak valid!]";
            system("pause");
            continue;
        } else {
            break;
        }
    }
    string resi = resiGen(pengirim.telp);
    string waktuKirim = getWaktuNow();
    string statusTime = waktuKirim; 
    StatusType statusType = CLIENT_IN; //default status type
    if (pengirim.alamat != "SURABAYA"){
        statusType = TRANSIT_IN;
    }

    system("cls");
    cout << "===== Data Paket =====\n";
    cout << "Resi           : " << resi << endl;
    cout << "Kategori       : " << kategoriBarang << endl;
    cout << "Berat          : " << beratStr << " Kg" << endl;
    cout << "Pengirim       : " << pengirim.nama << endl;
    cout << "Telp Pengirim  : " << pengirim.telp << endl;
    cout << "Alamat Pengirim: " << pengirim.alamat << endl;
    cout << "Penerima       : " << penerima.nama << endl;
    cout << "Telp Penerima  : " << penerima.telp << endl;
    cout << "Alamat Tujuan  : " << penerima.alamat << endl;
    cout << "=======================\n";
    cout << "Pastikan data sudah benar sebelum mengirim paket.\n";
    cout << "\n\nKonfirmasi? (Y/N): ";

    choice = inputHandler();
    if (choice != 'Y' && choice != 'y') {
        cout << "\n\n[Paket tidak jadi dikirim]";
        system("pause");
        return;
    }

    Paket* newPaket = new Paket(resi, kategoriBarang, waktuKirim, beratStr, statusType, statusTime, pengirim, penerima);
    //add to hash
    addToHash(newPaket);
    //add to queue
    enqueueHelper(newPaket);
}

void menuQueue(NodeQueue*& head, string title) {
    do {
        system("cls");
        cout << "===== " << title << " =====" << endl;
        if(!head) {
            cout << "\nAntrean kosong, tidak ada paket untuk diproses.";
            system("pause");
            return;
        }
        showQueue(head);
        cout << "\n1. Dequeue All\n2. Dequeue One\n0. Kembali ke Menu Utama";
        cout << "\nPilihan: ";

        char choice = inputHandler();
        switch (choice) {
            case '1':
                while (head != nullptr) {
                dequeueHelper(head->data);
                }
                cout << "\nSemua paket telah keluar dari antrian.";
                system("pause");
                continue;
            case '2':
                dequeueHelper(head->data);
                cout << "\nPaket telah dihapus dari antrian.";
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!";
                system("pause");
                continue;;
        }
    } while(true);
}

void menuGudang() {
    do{
        system("cls");
        cout << "===== Gudang Kirim Aja DC Rungkut =====\n";
        if (rootTree == nullptr || rootTree->child.empty()) {
            cout << "Gudang kosong, tidak ada paket yang tersedia.\n";
            system("pause");
            return;
        }
        showTree(rootTree);
        cout << "\n1. Kirim semua paket\n2. Hapus Paket dari Gudang\n0. Kembali ke Menu Utama\nPilihan: ";

        char choice = inputHandler();
        string resi;
        Paket* data = nullptr;
        switch (choice) {
            case '1':
                enqueueFromTree();
                cout << "\nPaket telah masuk ke antrian.";
                system("pause");
                continue;
            case '2':
                cout << "\nMasukkan Resi Paket yang ingin dihapus: ";
                cin >> resi;
                data = findInHash(resi);
                if (data) {
                    deleteFromTree(data);
                    deleteFromHash(data);
                    cout << "\nPaket berhasil dihapus dari Gudang.";
                } else {
                    cout << "\nPaket tidak ditemukan.";
                }
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!";
                system("pause");
                continue;
        }
    } while (true);
}

void menuHistory() {
    do{
        system("cls");
        cout << "===== History Kirim Aja DC Rungkut =====\n";
        if (headNodeHistory == nullptr) {
            cout << "History kosong, tidak ada paket yang tersedia.\n";
            system("pause");
            return;
        }
        showHistory(headNodeHistory);
        cout << "\n1. Kembalikan Paket\n2. Bersihkan History\n0. Kembali ke Menu Utama\nPilihan: ";

        char choice = inputHandler();
        switch (choice) {
            case '1': {
                if (headNodeHistory) {
                    popHistory(headNodeHistory->data);
                    cout << "\nPaket berhasil dikembalikan.";
                }
                system("pause");
                continue;
            }
            case '2':
                clearHistory();
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!";
                system("pause");
                continue;
        }
    }while(true);
}

void quit() {
    cout << "\n\nSaving...." << endl;
    // savePaketDB();
    cout << "\n\nProgram End." << endl;
}

void menu(MenuType menuType){
    system("cls");
    char choice;
    switch (menuType)
    {
    case ADMIN_MENU:
        cout <<"===== Kirim Aja DC Rungkut =====\n";
        cout << "1. Cek Resi\n";
        cout << "2. Kirim paket\n"; //paket baru
        cout << "3. Konfirmasi Request\n"; //CLIENT_IN
        cout << "4. Paket transit masuk\n"; //TRANSIT_IN
        cout << "5. Cek Gudang\n"; //Kosongin gudang
        cout << "6. Paket transit keluar\n"; //TRANSIT_OUT
        cout << "7. Konfirmasi Paket sampai\n"; //CLIENT_OUT
        cout << "8. Lihat History\n";
        cout << "0. Keluar\n";
        cout << "Pilihan: ";
        

        choice = inputHandler(); 
        switch (choice)
        {
        case '1':
            menuCekResi();
            break;
        case '2':
            menuKirimPaket();
            break;
        case '3':
            menuQueue(headClientIn, "Konfirmasi Request Kirim");
            break;
        case '4':
            menuQueue(headTransitIn, "Paket Transit Masuk");
            break;
        case '5':
            menuGudang();
            break;
        case '6':
            menuQueue(headTransitOut, "Paket Transit Keluar");
            break;
        case '7':
            menuQueue(headClientOut, "Konfirmasi Paket Sampai");
            break;
        case '8':
            menuHistory();
            break;
        case '0':
            
            return;
        default:
            cout << "Pilihan invalid!\n";
            system("pause");
            menu(ADMIN_MENU);
            break;
        }

        break;
    case USER_MENU:
        cout << "===== Kirim Aja DC Rungkut =====\n";
        cout << "1. Cek Resi\n";
        cout << "2. Kirim Paket\n"; //req kirim paket
        cout << "0. Keluar\n";
        cout << "Pilihan: ";


        choice = inputHandler();
        switch (choice)
        {
        case '1':
            menuCekResi();
            break;
        case '2':
            menuKirimPaket();
            break;
        case '0':
            
            return;
        default:
            cout << "Pilihan invalid!\n";
            system("pause");
            menu(USER_MENU);
            break;
        }
        break;

    case MAIN_MENU:

        cout << "===== Kirim Aja DC Rungkut =====\n";
        cout << "1. Petugas\n2. Pengguna\n0. Keluar\n";
        cout << "Pilihan: ";


        choice = inputHandler();
        switch (choice)
        {
        case '1':
            menu(ADMIN_MENU);
            break;
        case '2':
            menu(USER_MENU);
            break;
        case '0':
            quit();
            return;
        default:
            cout << "Pilihan invalid!\n";
            system("pause");
            menu(MAIN_MENU);
            break;
        }
    default:
        break;
    }
}

//==


void init() {
    initPaketDB();
    initTreeDB();
    menu(MAIN_MENU);
}

int main() {
    srand(time(0));
    
    atexit(quit);
    init();

    // string no = "0812345678901";
    // string resi = resiGen(no);
    // cout << resi <<endl;
    // cout << hashFunc(resi);

    // initGraphDB();      // initialize the graph from your files
    // showGraph();
    // cout << getOngkir(5, "SURABAYA", "YOGYAKARTA") << endl;
    return 0;
}
