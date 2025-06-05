#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
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

//stack, history sampai ke pelanggan dan transit keluar
struct NodeHistory{
    Paket* data {nullptr};
    NodeHistory* next{nullptr};

    NodeHistory(Paket* d):
    data(d){};
};
NodeHistory* headNodeHistory = nullptr; //top of the stack
NodeHistory* headNodeDelivered = nullptr; 

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
void pushHistory(Paket* data, NodeHistory*& head);
void popHistory(Paket*& data, NodeHistory*& head);
void addToTree(Paket*& data);
void deleteFromTree(Paket*& data);
void addToHash(Paket*& data);
void deleteFromHash(Paket*& data);
void jarakGraph(); //graph shortest path from a to b
int getJarak(const string& asal, const string& tujuan);
void updatePaketDB();

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
//helper date YYYY-MM-DD
string getDateNow(){
    time_t now = time(0); //get current time in seconds based on unix time
    tm *tmNow = localtime(&now); //struct 
    char date[11]; // "YYYY-MM-DD" + '\0'
    strftime(date, sizeof(date), "%Y-%m-%d", tmNow);

    return date;
}
//helper status YYYY-MM-DD <HH:MM>
string setStatusTime(){
    string status = "[" + getDateNow() + " <" + getWaktuNow() + ">]";
    return status;
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

    statusPaket = data->statusTime;
    statusPaket = statusPaket + " - " + statusStr;
    return statusPaket;
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
        return regex_match(inputStr, regex("^[0-9]{10,13}$"));
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

void addToHash(Paket*& data){
    int hashIndex = hashFunc(data->resi);
    NodeHash* newNode = new NodeHash(data);

    if (paketHash[hashIndex].head == nullptr) {
        paketHash[hashIndex].head = newNode;
    } else {
        NodeHash* temp = paketHash[hashIndex].head;
        while(temp->next != nullptr){
            if(temp->dataPtr->resi == data->resi){
                cout << "\n\n[Paket dengan resi ini sudah ada di hash]\n";
                delete newNode; //delete the new node since it's a duplicate
                return;
            }
            temp = temp->next;
        }
        temp->next = newNode;
    }
    cout << "\n[Paket berhasil ditambahkan]";
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
        cout << "\n\n[Paket tidak ditemukan]\n";
        return nullptr;
    }
    //found
    cout << "\n[Paket ditemukan]\n";
    return helper->dataPtr;
};

//Queue function, pass pointer by reference to update the global variable
void enqueuePaket(Paket* data, NodeQueue*& head, NodeQueue*& tail){
    if(data == nullptr){
        cout << "\n\nData Paket Kosong!\n";
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
void enqueueHelper(Paket*& data){
    // cout << "[DEBUG] Enqueueing paket: " << data->resi << endl;
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
            cout << "\n\n[Paket " << data->resi << " keluar gudang]\n";
            data->statusType = TRANSIT_OUT; //paket sudah disortir
        } else {
            enqueuePaket(data, headClientOut, tailClientOut); //paket diantar ke penerima
            cout << "\n\n[Paket " << data->resi << " diantar ke penerima]\n";
            data->statusType = CLIENT_OUT; //paket dalam proses pengantaran
        }
        break;
    case RETURNED:
        if (data->penerima.alamat != "SURABAYA"){
            enqueuePaket(data, headTransitOut, tailTransitOut); //paket keluar gudang
            data->statusType = TRANSIT_OUT; //paket sudah disortir
        } else {
            enqueuePaket(data, headClientOut, tailClientOut); //paket diantar ke penerima
            data->statusType = CLIENT_OUT; //paket dalam proses pengantaran
        }
        break;
    default:
        break;
    }

}

void dequeuePaket(NodeQueue*& head, NodeQueue*& tail){
    if(head == nullptr || head->data == nullptr){
        cout << "\nGagal menghapus, data antrian kosong!\n";
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

//helper untuk dequeue paket sesuai statusnya
void dequeueHelper(Paket*& data){
    data->statusTime = setStatusTime(); //update status time
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
        pushHistory(data, headNodeDelivered); //paket diantar ke penerima
        dequeuePaket(headClientOut, tailClientOut); //paket dihapus dari antrian
        data->statusType = DELIVERED;
        break;
    case TRANSIT_OUT:
        pushHistory(data, headNodeHistory); //Simpan History
        dequeuePaket(headTransitOut, tailTransitOut); //paket dihapus dari antrian
        data->statusType = TRANSIT_ACC; //paket sudah disortir
        break;
    default:
        break;
    }
};

//sort paket pada antrian, the older waktuKirim the higher priority, insertion sort since the data is almost sorted most of the time
void sortPaketByWaktuKirim(NodeQueue*& head, NodeQueue*& tail){
    if (head == nullptr || head->next == nullptr) {
        cout << "\n\n[Data antrian kosong atau hanya ada satu paket, tidak perlu diurutkan.]\n";
        return;
    }

    NodeQueue* sorted = nullptr; //sorted list
    NodeQueue* current = head;

    while (current != nullptr) {
        NodeQueue* nextNode = current->next; // Store the next node
        // Insert current into sorted linked list
        if (sorted == nullptr || current->data->waktuKirim < sorted->data->waktuKirim) {
            current->next = sorted;
            sorted = current;
        } else {
            NodeQueue* temp = sorted;
            while (temp->next != nullptr && temp->next->data->waktuKirim < current->data->waktuKirim) {
                temp = temp->next;
            }
            current->next = temp->next;
            temp->next = current;
        }
        current = nextNode; // Move to the next node
    }

    head = sorted; // Update head to the new sorted list
    tail = head; // need to find it again
    while (tail != nullptr && tail->next != nullptr) {
        tail = tail->next;
    }
    cout << "\n[Antrian paket telah diurutkan berdasarkan waktu kirim.]\n";
};

//stack operation
void pushHistory(Paket* data, NodeHistory*& head){
    NodeHistory* newNode = new NodeHistory(data);
    //this point at nullptr if stack is empty, so no prob here
    newNode->next = head; 
    head = newNode;
    return;
};

void popHistory(Paket*& data, NodeHistory*& head){
    NodeHistory* delNode = head;

    head = delNode->next;

    deleteFromHash(delNode->data); //delete from hash
    delete delNode;
    return;
}

void returnPaket(string resi){
    if (headNodeDelivered == nullptr) {
        cout << "\n\n[History kosong!]\n";
        system("pause");
        return;
    }
    if(findInHash(resi) == nullptr){
        cout << "\n\n[Paket dengan Resi " << resi << " tidak ditemukan.]\n";
        system("pause");
        return;
    }
    
    // Find the delivered paket by resi
    NodeHistory* helper = headNodeDelivered;
    NodeHistory* prev = nullptr;
    while (helper != nullptr) {
        if (helper->data->resi == resi) {
            // Paket ditemukan hapus dari history
            //unlink the node
            if(helper == headNodeDelivered) {
                headNodeDelivered = helper->next; 
            } else {
                prev->next = helper->next;
            }

            //lakukan pengembalian
            helper->data->statusType = RETURNED;
            helper->data->statusTime = setStatusTime();
            DataClient temp = helper->data->pengirim;
            helper->data->pengirim = helper->data->penerima; //pengirim jadi penerima
            helper->data->penerima = temp;

            addToTree(helper->data); //kembalikan ke gudang
            delete helper; //delete the node
            cout << "\n[Paket berhasil dikembalikan ke Gudang]\n";
            return;
        }
        prev = helper; 
        helper = helper->next;
    }
    // not found dalam history
    cout << "\n[Paket dengan Resi " << resi << " tidak ditemukan dalam history.]";
    system("pause");
    return;
}

//clear all history
void clearHistory(NodeHistory*& head){
    if (head == nullptr) {
        cout << "\n\nHistory kosong!\n";
        system("pause");
        return;
    }
    NodeHistory* helper = head;
    while (helper != nullptr) {
        NodeHistory* temp = helper;
        helper = helper->next;
        deleteFromHash(temp->data); //delete from hash
        delete temp; //delete node
    }
    head = nullptr;
    cout << "\n[History telah dibersihkan.]";
};

//traverse and print list
void showQueue(NodeQueue* head){
    if (head == nullptr) {
        cout << "\n\nData antrian kosong!\n";
        system("pause");
        return;
    }

    NodeQueue* helper = head;
    int num = 1;
    while(helper != nullptr) {
        cout << "[" << num << "]" << endl;
        cout << "  Resi             : " << helper->data->resi << endl;
        cout << "  Kategori Barang  : " << helper->data->kategoriBarang << endl;
        cout << "  Pengirim         : " << helper->data->pengirim.nama << endl;
        cout << "  Telp Pengirim    : " << helper->data->pengirim.telp << endl;
        cout << "  Alamat Pengirim  : " << helper->data->pengirim.alamat << endl;
        cout << "  Penerima         : " << helper->data->penerima.nama << endl;
        cout << "  Telp Penerima    : " << helper->data->penerima.telp << endl;
        cout << "  Alamat Tujuan    : " << helper->data->penerima.alamat << endl;
        cout << "  Kategori Barang  : " << helper->data->kategoriBarang << endl;
        cout << "  Waktu Kirim      : " << helper->data->waktuKirim << endl;
        cout << "  Status           : " << getStatus(helper->data) << endl;
        cout << "----------------------------------------" << endl;

        helper = helper->next;
        num++;
    }
};

void showHistory(NodeHistory* head){
    if (head == nullptr) {
        cout << "\n\nData History kosong!\n";
        system("pause");
        return;
    }

    NodeHistory* helper = head;
    int num = 1;
    while(helper != nullptr) {
        cout << "[" << num << "]" << endl;
        cout << "  Resi             : " << helper->data->resi << endl;
        cout << "  Kategori Barang  : " << helper->data->kategoriBarang << endl;
        cout << "  Pengirim         : " << helper->data->pengirim.nama << endl;
        cout << "  Telp Pengirim    : " << helper->data->pengirim.telp << endl;
        cout << "  Alamat Pengirim  : " << helper->data->pengirim.alamat << endl;
        cout << "  Penerima         : " << helper->data->penerima.nama << endl;
        cout << "  Telp Penerima    : " << helper->data->penerima.telp << endl;
        cout << "  Alamat Tujuan    : " << helper->data->penerima.alamat << endl;
        cout << "  Kategori Barang  : " << helper->data->kategoriBarang << endl;
        cout << "  Status           : " << getStatus(helper->data) << endl;
        cout << "----------------------------------------" << endl;

        helper = helper->next;
        num++;
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

void addToTree(Paket*& data){
    bool found = false;
    if(data == nullptr){
        cout << "\n\nData masih kosong.";
        return;
    }
    NodeTree* newChild = new NodeTree(data->resi, data);
    //hwoh
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

void deleteFromTree(Paket*& data){
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
        cout << "\n\nPaket " << data->resi << " tidak ditemukan";
    }
    if(found){
        // cout << "[DEBUG] Deleted from tree: " << data->resi << endl;
    }
    return;
};

void enqueueFromTree() {
    if (rootTree == nullptr) {
        cout << "\n\nTree masih kosong.";
        return;
    }
    bool isEmpty = true;
    // Traverse the tree and enqueue all Paket nodes
    vector<Paket*> toProcess;
    for (NodeTree* kategoriNode : rootTree->child) {
        for (NodeTree* paketNode : kategoriNode->child) {
            if (paketNode->dataPtr != nullptr) {
                isEmpty = false;
                toProcess.push_back(paketNode->dataPtr);
            }
        }
    }
    
    if (isEmpty) {
        cout << "\n\n[Gudang kosong]\n";
        return;
    }

    for (Paket* p : toProcess) {
        enqueueHelper(p);
        deleteFromTree(p);
    }
    cout << "\nPaket telah masuk ke antrian.";
}

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

//helper show kota
void showGraph() {
    cout << "\n=== Daftar Kota ===\n";
    int i = 1;
    for (auto node : kotaGraph) {
        cout << "[" << i << "] " << node->kota << endl;
        i++;
    }
    cout << "================\n";
}

//Dijkstra Shortest Path
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
            pushHistory(paket, headNodeDelivered);
            break;
        case TRANSIT_ACC:
            pushHistory(paket, headNodeHistory);
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
            node = node->next;
        }
    }
    tulis.close();
    cout << "\n[Data paket berhasil disimpan ke file]\n";
};

void importBulkDB(string filename){
    ifstream baca("./data/" + filename);
    if (baca.fail()){
        cerr << "\n\nGagal membaca file bulk kirim";
        return exit(0);
    }

    string line;
    while(getline(baca, line)) {
        stringstream ss(line);
        Paket* paket = new Paket;
        string statusStr;

        // Format file: kategoriBarang|waktuKirim|berat|statusType|pengirim_nama|pengirim_telp|pengirim_alamat|penerima_nama|penerima_telp|penerima_alamat
        getline(ss, paket->kategoriBarang, '|');
        getline(ss, paket->waktuKirim, '|');
        getline(ss, paket->berat, '|');
        getline(ss, paket->pengirim.nama, '|');
        getline(ss, paket->pengirim.telp, '|');
        getline(ss, paket->pengirim.alamat, '|');
        getline(ss, paket->penerima.nama, '|');
        getline(ss, paket->penerima.telp, '|');
        getline(ss, paket->penerima.alamat, '|');
        // Generate resi
        paket->resi = resiGen(paket->pengirim.telp);
        paket->statusTime = setStatusTime(); //set status time to now
        if(paket->pengirim.alamat == "SURABAYA"){
            paket->statusType = CLIENT_IN; //pengirim dari surabaya
        } else {
            paket->statusType = TRANSIT_IN; //transit
        }
        // Add to hash map
        addToHash(paket);

        enqueueHelper(paket); //enqueue sesuai statusnya
    }
    baca.close();
    cout << "\n[Data bulk kirim berhasil ditambahkan]\n";
}

//CLI 

void menuCekResi(){
    system("cls");
    string resi;
    cout << "===== Cek Resi =====\n";
    cout << "Resi : ";
    cin >> resi;
    //regex input invalid return
    if(!regexValidator(RESI, resi)){
        cout << "\n\n[Resi tidak valid!]\n";
        system("pause");
        return;
    }

    Paket* data;
    data = findInHash(resi);
    if(data){
        system("cls");
        cout << "\n===== Detail Paket =====\n";
        cout << "Resi           : " << data->resi << endl;
        cout << "Kategori       : " << data->kategoriBarang << endl;
        cout << "Berat          : " << data->berat << " Kg" << endl;
        cout << "Tanggal Kirim  : " << data->waktuKirim << endl;
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
        cout << "\nKategori Barang (0 untuk batal): ";
        cin >> pilihan;
        if (pilihan == 0) {
            return;
        }
        if (pilihan < 1 || pilihan > rootTree->child.size()) {
            cout << "\n\n[Kategori tidak valid!]\n";
            system("pause");
            continue; //invalid input, continue to loop
        }
        kategoriBarang = rootTree->child[pilihan-1]->key;
        //berat
        cout << "Berat (Kg): ";
        cin >> berat;
        //validasi berat
        if (berat <= 0 || berat > 100) { //max 100KG
            cout << "\n\n[Berat tidak valid!]\n";
            system("pause");
            continue;;
        }
        beratStr = to_string(berat);
        //list kota        
        showGraph();
        
        //Harusnya always SURABAYA tapi untuk simulasi paket datang dari kota lain jadi masi dibolehin
        cout << "\nAlamat Pengirim: ";
        cin >> pilihan; 
        if (pilihan < 1 || pilihan > kotaGraph.size()) {
            cout << "\n\n[Kota tidak valid!]\n";
            system("pause");
            continue; //invalid input, continue to loop
        }
        alamatKirim = kotaGraph[pilihan-1]->kota; 
        cout << "Alamat Penerima: "; 
        cin >> pilihan;
        cin.ignore(); 
        if (pilihan < 1 || pilihan > kotaGraph.size()) {
            cout << "\n\n[Kota tidak valid!]\n";
            system("pause");
            continue; //invalid input, continue to loop
        }
        alamatTujuan = kotaGraph[pilihan-1]->kota;
        
        break;
    }
    //cek ongkir
    ongkir = getOngkir(berat, alamatKirim, alamatTujuan);
    cout << "Biaya Ongkir : Rp. " << ongkir;
    char choice;
    cout << "\n\nKonfirmasi? (Y/N): ";

    choice = inputHandler();
    if (choice != 'Y' && choice != 'y') {
        cout << "\n\n[Paket tidak jadi dikirim]\n";
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
            cout << "\n\n[Input tidak valid!]\n";
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
            cout << "\n\n[Input tidak valid!]\n";
            system("pause");
            continue;
        } else {
            break;
        }
    }
    string resi = resiGen(pengirim.telp);
    string waktuKirim = getDateNow();
    string statusTime = setStatusTime();
    StatusType statusType = CLIENT_IN; //default status type
    if (pengirim.alamat != "SURABAYA"){
        statusType = TRANSIT_IN;
    }

    system("cls");
    cout << "===== Data Paket =====\n";
    cout << "Resi           : " << resi << endl;
    cout << "Kategori       : " << kategoriBarang << endl;
    cout << "Berat          : " << berat << " Kg" << endl;
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
        cout << "\n\n[Paket tidak jadi dikirim]\n";
        system("pause");
        return;
    }
    cout << "\n\n[Mengirim paket...]\n";
    Paket* newPaket = new Paket(resi, kategoriBarang, waktuKirim, beratStr, statusType, statusTime, pengirim, penerima);
    //add to hash
    cout << "\n[Menambahkan Data...]";
    addToHash(newPaket);
    //add to queue
    cout << "\n\n[Mengirim paket ke antrian...]";
    enqueueHelper(newPaket);

    cout << "\n\n[Paket berhasil dikirim!]\n";
    system("pause");
    return;
}

void menuQueue(NodeQueue*& head, NodeQueue*& tail, string title) {
    do {
        system("cls");
        cout << "===== " << title << " =====" << endl;
        if(!head) {
            cout << "\nAntrean kosong, tidak ada paket untuk diproses.\n";
            system("pause");
            return;
        }
        showQueue(head);
        cout << "\n1. Dequeue All\n2. Dequeue One\n3. Sort Paket\n0. Kembali ke Menu Utama";
        cout << "\nPilihan: ";

        char choice = inputHandler();
        switch (choice) {
            case '1':
                while (head != nullptr) {
                dequeueHelper(head->data);
                }
                cout << "\nSemua paket telah keluar dari antrian.\n";
                system("pause");
                continue;
            case '2':
                dequeueHelper(head->data);
                cout << "\nPaket telah dihapus dari antrian.\n";
                system("pause");
                continue;
            case '3':
                sortPaketByWaktuKirim(head, tail);
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!\n";
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
                    cout << "\nPaket tidak ditemukan.\n";
                }
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!\n";
                system("pause");
                continue;
        }
    } while (true);
}

void menuReturnPaket(){
    do{
        system("cls");
        cout << "===== History Delivered Kirim Aja DC Rungkut =====\n";
        if (headNodeDelivered == nullptr) {
            cout << "History kosong, tidak ada paket yang tersedia.\n";
            system("pause");
            return;
        }
        showHistory(headNodeDelivered);
        cout << "\n1. Return Paket\n2. Bersihkan History\n0. Kembali ke Menu Utama\nPilihan: ";
        
        char choice = inputHandler();
        string resi;
        switch (choice) {
            case '1': {
                if (headNodeDelivered) {
                    cout << "\nMasukkan Resi Paket yang ingin dikembalikan: ";
                    cin >> resi;
                    //regex input invalid return
                    if(!regexValidator(RESI, resi)){
                        cout << "\n\n[Resi tidak valid!]\n";
                        system("pause");
                        continue;
                    }
                    returnPaket(resi);
                }
                system("pause");
                continue;
            }
            case '2':
                clearHistory(headNodeDelivered);
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!\n";
                system("pause");
                continue;
        }
    }while(true);
};

void menuHistory() {
    do{
        system("cls");
        cout << "===== History Transit Kirim Aja DC Rungkut =====\n";
        if (headNodeHistory == nullptr) {
            cout << "History kosong, tidak ada paket yang tersedia.\n";
            system("pause");
            return;
        }
        showHistory(headNodeHistory);
        cout << "\n1. Hapus history terbaru\n2. Bersihkan History\n0. Kembali ke Menu Utama\nPilihan: ";

        char choice = inputHandler();
        switch (choice) {
            case '1': {
                if (headNodeHistory) {
                    popHistory(headNodeHistory->data, headNodeHistory);
                    cout << "\nPaket berhasil dihapus dari history.\n";
                }
                system("pause");
                continue;
            }
            case '2':
                clearHistory(headNodeHistory);
                system("pause");
                continue;
            case '0':
                return; // Kembali ke menu utama
            default:
                cout << "\nPilihan tidak valid!\n";
                system("pause");
                continue;
        }
    }while(true);
}

void menuImportBulk() {
    string filename;
    system("cls");
    cout << "===== Kirim Bulk =====\n";
    cout << "Pastikan file txt sudah tersedia.\n";
    cout << "Masukan nama file (tanpa .txt) (0 untuk batal): ";
    cin >> filename;
    if (filename == "0") {
        return;
    }
    filename += ".txt";
    importBulkDB(filename);

    system("pause");
    return;
}

void quit() {
    cout << "\n\nSaving...." << endl;
    savePaketDB();
    cout << "\n\nProgram End." << endl;
    exit(0);
}

void menu(MenuType menuType){
    while (true) {
        system("cls");
        char choice;
        switch (menuType)
        {
        case ADMIN_MENU:
            cout <<"===== Kirim Aja DC Rungkut =====\n";

            cout << "1. Antrian Konfirmasi Request\n"; //CLIENT_IN
            cout << "2. Antrian Transit Masuk\n"; //TRANSIT_IN
            cout << "3. Cek Gudang\n"; //Tree
            cout << "4. Antrian Paket Transit Keluar\n"; //TRANSIT_OUT
            cout << "5. Antrian Konfirmasi Paket Sampai\n"; //CLIENT_OUT
            cout << "6. Return Paket\n"; //History Delivered
            cout << "7. History Paket Keluar\n";
            cout << "8. Kirim bulk\n";
            cout << "0. Keluar\n";
            cout << "Pilihan: ";
            

            choice = inputHandler(); 
            switch (choice)
            {
            
            case '1':
                menuQueue(headClientIn, tailClientIn, "Konfirmasi Request Kirim");
                menu(ADMIN_MENU);
                break;
            case '2':
                menuQueue(headTransitIn, tailTransitIn, "Paket Transit Masuk");
                menu(ADMIN_MENU);
                break;
            case '3':
                menuGudang();
                menu(ADMIN_MENU);
                break;
            case '4':
                menuQueue(headTransitOut, tailTransitOut, "Paket Transit Keluar");
                menu(ADMIN_MENU);
                break;
            case '5':
                menuQueue(headClientOut, tailClientOut, "Konfirmasi Paket Sampai");
                menu(ADMIN_MENU);
                break;
            case '6':
                menuReturnPaket();
                menu(ADMIN_MENU);
                break;
            case '7':
                menuHistory();
                menu(ADMIN_MENU);
                break;
            case '8':
                menuImportBulk();
                menu(ADMIN_MENU);
                break;
            case '0':
                menu(MAIN_MENU);
                return;
            default:
                cout << "Pilihan invalid!\n";
                system("pause");
                menu(ADMIN_MENU);
                break;
            }

            break;

        case MAIN_MENU:
            system("cls");
            cout << "===== Selamat Datang di =====\n";        
            cout << "    __ __ _      _              ___      _          \n";
            cout << "   / //_/(_)____(_)___ ___     /   |    (_)___ _    \n";
            cout << "  / ,<  / / ___/ / __ `__ \\   / /| |   / / __ `/   \n";
            cout << " / /| |/ / /  / / / / / / /  / ___ |  / / /_/ /     \n";
            cout << "/_/ |_/_/_/  /_/_/ /_/ /_/  /_/  |_|_/ /\\__,_/      \n";
            cout << "                                  /___/             \n";
            cout << "===== DC Rungkut =====\n";
            cout << "1. Cek Resi\n2. Kirim Paket\n3. Manage\n0. Keluar\n";
            cout << "Pilihan: ";

            choice = inputHandler();
            switch (choice)
            {
            case '1':
                menuCekResi();
                menu(MAIN_MENU);
                break;
            case '2':
                menuKirimPaket();
                menu(MAIN_MENU);
                break;
            case '3':
                menu(ADMIN_MENU);
                break;
            case '0':
                quit();
                break;
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
}

//
void init() {
    initGraphDB();
    initTreeDB();
    initPaketDB();
    menu(MAIN_MENU);
}

int main() {
    atexit(quit);
    srand(time(0));
    init();

    return 0;
}
