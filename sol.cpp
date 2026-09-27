#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <functional>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>


//==================== UTILS ===============================
time_t now_ts() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

std::string formatTime(time_t t) {
    if (t == 0) return "N/A";
    tm tm_buf;
#if defined(_MSC_VER)
    localtime_s(&tm_buf, &t);
#else
    tm *tmp = localtime(&t);
    if (!tmp) return "N/A";
    tm_buf = *tmp;
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buf);
}

//==================== TREE NODE ============================
struct TreeNode {
    int version_id;
    std::string content;
    std::string message;
    time_t created_timestamp;
    time_t snapshot_timestamp;
    TreeNode* parent;
    std::vector<TreeNode*> children;

    TreeNode(int vid = 0) {
        version_id = vid;
        content = "";
        message = "";
        created_timestamp = now_ts();
        snapshot_timestamp = 0;
        parent = nullptr;
    }

    bool is_snapshot() const { return snapshot_timestamp != 0; }
};

//==================== VERSION HASH MAP =======================
class VersionHashMap {
private:
    struct Node {
        int key;
        TreeNode* val;
        Node* next;
        Node(int k, TreeNode* v): key(k), val(v), next(nullptr) {}
    };
    std::vector<Node*> table;
    int capacity;
    int count;

    int hashfn(int key) const { return key < 0 ? -key % capacity : key % capacity; }

    void rehash_if_needed() {
        if ((double)(count+1)/capacity > 0.7) {
            int newcap = capacity*2+1;
            std::vector<Node*> newtable(newcap,nullptr);
            for(int i=0;i<capacity;++i){
                Node* cur = table[i];
                while(cur){
                    Node* nxt = cur->next;
                    int idx = cur->key<0?-cur->key:newcap;
                    idx = cur->key % newcap;
                    cur->next = newtable[idx];
                    newtable[idx] = cur;
                    cur = nxt;
                }
            }
            table.swap(newtable);
            capacity = newcap;
        }
    }

public:
    VersionHashMap(int cap=127) {
        capacity = std::max(11, cap);
        table.assign(capacity,nullptr);
        count = 0;
    }

    ~VersionHashMap() {
        for(int i=0;i<capacity;++i){
            Node* cur = table[i];
            while(cur){
                Node* nxt = cur->next;
                delete cur;
                cur = nxt;
            }
        }
    }

    void put(int key, TreeNode* val){
        rehash_if_needed();
        int idx = hashfn(key);
        Node* cur = table[idx];
        while(cur){ if(cur->key==key){ cur->val = val; return;} cur=cur->next; }
        Node* n = new Node(key,val);
        n->next = table[idx]; table[idx]=n; ++count;
    }

    TreeNode* get(int key) const {
        int idx = hashfn(key);
        Node* cur = table[idx];
        while(cur){ if(cur->key==key) return cur->val; cur=cur->next; }
        return nullptr;
    }

    bool contains(int key) const { return get(key)!=nullptr; }
};

//==================== STRING HASH MAP ======================
template<typename V>
class StringHashMap {
private:
    struct Node {
        std::string key;
        V val;
        Node* next;
        Node(const std::string &k, V v): key(k), val(v), next(nullptr){}
    };
    std::vector<Node*> table;
    int capacity;
    int count;

    unsigned long hashfn_str(const std::string &s) const{
        unsigned long h = 5381;
        for(char c:s) h = ((h<<5)+h)+(unsigned char)c;
        return h;
    }

    int idx_for(const std::string &k) const{ return (int)(hashfn_str(k)%capacity); }

    void rehash_if_needed(){
        if((double)(count+1)/capacity>0.7){
            int newcap = capacity*2+1;
            std::vector<Node*> newtable(newcap,nullptr);
            for(int i=0;i<capacity;++i){
                Node* cur = table[i];
                while(cur){
                    Node* nxt = cur->next;
                    int idx = idx_for(cur->key) % newcap;
                    cur->next = newtable[idx]; newtable[idx]=cur; cur=nxt;
                }
            }
            table.swap(newtable);
            capacity=newcap;
        }
    }

public:
    StringHashMap(int cap=127){ capacity=std::max(11,cap); table.assign(capacity,nullptr); count=0;}
    ~StringHashMap(){ for(int i=0;i<capacity;++i){ Node* cur=table[i]; while(cur){ Node* nxt=cur->next; delete cur; cur=nxt;} } }

    void put(const std::string &k, V v){ rehash_if_needed(); int idx=idx_for(k); Node* cur=table[idx]; while(cur){ if(cur->key==k){ cur->val=v; return;} cur=cur->next;} Node* n=new Node(k,v); n->next=table[idx]; table[idx]=n; ++count; }
    V get(const std::string &k) const{ int idx=idx_for(k); Node* cur=table[idx]; while(cur){ if(cur->key==k) return cur->val; cur=cur->next;} return (V)0; }
    bool contains(const std::string &k) const {
    int idx = idx_for(k);
    Node* cur = table[idx];
    while(cur){
        if(cur->key == k) return true; 
        cur = cur->next;
    }
    return false;
}

    void remove(const std::string &k){ int idx=idx_for(k); Node* cur=table[idx]; Node* prev=nullptr; while(cur){ if(cur->key==k){ if(prev) prev->next=cur->next; else table[idx]=cur->next; delete cur; --count; return;} prev=cur; cur=cur->next;} }
    std::vector<std::pair<std::string,V>> items() const{
        std::vector<std::pair<std::string,V>> out;
        for(int i=0;i<capacity;++i){ Node* cur=table[i]; while(cur){ out.push_back({cur->key,cur->val}); cur=cur->next;} }
        return out;
    }
};

//==================== FILE VERSION TREE ====================
class FileVersionTree {
public: 
    int recent_priority;
    int biggest_priority;
    int priority;

private:
    std::string filename;
    TreeNode* root;
    TreeNode* active;
    VersionHashMap version_map;
    int total_versions;
    time_t last_modified;
    int access_count;

public:
    FileVersionTree(const std::string &fname): filename(fname), version_map(127){
        root = new TreeNode(0);
        root->content=""; root->message="Initial_empty_snapshot";
        root->snapshot_timestamp = now_ts(); root->created_timestamp=now_ts();
        version_map.put(0, root);
        active=root; total_versions=1; last_modified=now_ts(); access_count=0;
        recent_priority=0; biggest_priority=0; priority=0;
    }

    ~FileVersionTree(){
        std::function<void(TreeNode*)> del = [&](TreeNode* n){
            if(!n) return;
            for(TreeNode* ch:n->children) del(ch);
            delete n;
        };
        del(root);
    }

    std::string getName() const { return filename; }
    int getTotalVersions() const { return total_versions; }
    time_t getLastModified() const { return last_modified; }
    int getAccessCount() const { return access_count; }
    TreeNode* getActive() const { return active; }

    std::string read_print() { access_count++; std::cout<<active->content<<std::endl; return active->content; }

    void insert_cmd(const std::string &content){
        if(active->snapshot_timestamp!=0){
            TreeNode* node = new TreeNode(total_versions);
            node->content=active->content+content;
            node->created_timestamp=now_ts();
            node->parent=active;
            active->children.push_back(node);
            version_map.put(total_versions,node);
            std::cout<<"New version "<<total_versions<<" created for '"<<filename<<"'. Parent is version "<<active->version_id<<"."<<std::endl;
            active=node;
            total_versions++;
        } else {
            active->content+=content;
            active->created_timestamp=now_ts();
            std::cout<<"Modified active version "<<active->version_id<<" of '"<<filename<<"' in-place."<<std::endl;
        }
        last_modified=now_ts();
    }

    void update_cmd(const std::string &content){
        if(active->snapshot_timestamp!=0){
            TreeNode* node = new TreeNode(total_versions);
            node->content=content;
            node->created_timestamp=now_ts();
            node->parent=active;
            active->children.push_back(node);
            version_map.put(total_versions,node);
            std::cout<<"New version "<<total_versions<<" created for '"<<filename<<"'. Parent is version "<<active->version_id<<"."<<std::endl;
            active=node; total_versions++;
        } else {
            active->content=content;
            active->created_timestamp=now_ts();
            std::cout<<"Modified active version "<<active->version_id<<" of '"<<filename<<"' in-place."<<std::endl;
        }
        last_modified=now_ts();
    }

    void snapshot_cmd(const std::string &message){
        active->message=message;
        active->snapshot_timestamp=now_ts();
        std::cout<<"Snapshot created for active version "<<active->version_id<<" of '"<<filename<<"'."<<std::endl;
        last_modified=now_ts();
    }

    bool rollback_to(int vid){
        TreeNode* node = version_map.get(vid);
        if(!node){ std::cout<<"Error: Version "<<vid<<" not found for file '"<<filename<<"'."<<std::endl; return false; }
        active=node;
        std::cout<<"Active version for '"<<filename<<"' set to "<<vid<<"."<<std::endl;
        return true;
    }

    bool rollback_parent(){
        if(!active->parent){ std::cout<<"Error: Cannot rollback from root version."<<std::endl; return false;}
        active=active->parent;
        std::cout<<"Active version for '"<<filename<<"' set to parent version "<<active->version_id<<"."<<std::endl;
        return true;
    }

    void history_cmd(){
        std::vector<TreeNode*> snaps;
        TreeNode* cur = active;
        while(cur){ if(cur->snapshot_timestamp!=0) snaps.push_back(cur); cur=cur->parent;}
        std::cout<<"History for "<<filename<<":"<<std::endl;
        if(snaps.empty()){ std::cout<<"(no snapshots on path)"<<std::endl; return;}
        for(TreeNode* n:snaps){
            std::cout<<"Version "<<n->version_id<<": "<<formatTime(n->snapshot_timestamp)<<" - "<<n->message<<std::endl;
        }
    }

    bool has_version(int vid) const { return version_map.contains(vid); }
};

//==================== FILE HEAP ====================
class FileHeap {
private:
    std::vector<FileVersionTree*> heap;
    StringHashMap<int> index_map;

    int get_priority(FileVersionTree* f) const { return f->priority; }

    void swap_nodes(int i,int j){
        FileVersionTree* A = heap[i]; FileVersionTree* B=heap[j];
        std::swap(heap[i],heap[j]);
        index_map.put(A->getName(), j);
        index_map.put(B->getName(), i);
    }

    void sift_up(int i){
        while(i>0){
            int p=(i-1)/2;
            if(get_priority(heap[i])>get_priority(heap[p])){
                swap_nodes(i,p);
                i=p;
            } else break;
        }
    }

    void sift_down(int i){
        int n = heap.size();
        while(true){
            int l=2*i+1, r=2*i+2, largest=i;
            if(l<n && get_priority(heap[l])>get_priority(heap[largest])) largest=l;
            if(r<n && get_priority(heap[r])>get_priority(heap[largest])) largest=r;
            if(largest!=i){ swap_nodes(i,largest); i=largest; } else break;
        }
    }

public:
    FileHeap(): index_map(127) {}

    void add_or_update(FileVersionTree* f){
        std::string name = f->getName();
        if(index_map.contains(name)){
            int idx = index_map.get(name);
            heap[idx] = f;        
            sift_up(idx);
            sift_down(idx);
        } else {
            heap.push_back(f);
            index_map.put(name, heap.size()-1);
            sift_up(heap.size()-1);
        }
    }

    void remove(const std::string &name){
        if(!index_map.contains(name)) return;
        int idx=index_map.get(name);
        int last=heap.size()-1;
        if(idx!=last) swap_nodes(idx,last);
        heap.pop_back();
        index_map.remove(name);
        if(idx<(int)heap.size()){ sift_up(idx); sift_down(idx);}
    }

    std::vector<FileVersionTree*> top_n(int n) const {
        std::vector<FileVersionTree*> out;
        if(heap.empty() || n <= 0) return out;

        // Copy heap
        std::vector<FileVersionTree*> temp(heap);
        auto cmp = [](FileVersionTree* a, FileVersionTree* b){ return a->priority > b->priority; };
        std::make_heap(temp.begin(), temp.end(), cmp);  

        for(int i=0;i<n && !temp.empty();++i){
            std::pop_heap(temp.begin(), temp.end(), cmp);
            out.push_back(temp.back());
            temp.pop_back();
        }
        return out;
    }

    bool empty() const { return heap.empty(); }
};

//==================== FILE SYSTEM ====================
class FileSystem {
private:
    FileHeap recent_heap;
    FileHeap biggest_heap;
    int timestamp_counter = 0;

public:
    StringHashMap<FileVersionTree*> files;
    bool contains(const std::string &fname) const {
        return files.contains(fname);
    }

    void create_file(const std::string &fname){
        if(files.contains(fname)) return;
        FileVersionTree* f = new FileVersionTree(fname);
        files.put(fname,f);
        f->recent_priority=++timestamp_counter;
        f->biggest_priority=f->getTotalVersions();
        f->priority=f->recent_priority;
        recent_heap.add_or_update(f);
        f->priority=f->biggest_priority;
        biggest_heap.add_or_update(f);
    }

    void insert_into(const std::string &fname,const std::string &content){
        FileVersionTree* f = files.get(fname);
        f->insert_cmd(content);
        f->recent_priority=++timestamp_counter;
        f->biggest_priority=f->getTotalVersions();
        f->priority=f->recent_priority; recent_heap.add_or_update(f);
        f->priority=f->biggest_priority; biggest_heap.add_or_update(f);
    }

    void update_file(const std::string &fname,const std::string &content){
        FileVersionTree* f = files.get(fname);
        f->update_cmd(content);
        f->recent_priority=++timestamp_counter;
        f->biggest_priority=f->getTotalVersions();
        f->priority=f->recent_priority; recent_heap.add_or_update(f);
        f->priority=f->biggest_priority; biggest_heap.add_or_update(f);
    }

    void snapshot_file(const std::string &fname,const std::string &msg){
        FileVersionTree* f = files.get(fname);
        f->snapshot_cmd(msg);
        f->recent_priority=++timestamp_counter;
        f->priority=f->recent_priority; recent_heap.add_or_update(f);
    }

    void rollback_file(const std::string &fname,const std::string &version_str){
        FileVersionTree* f = files.get(fname);
        if(version_str.empty()) f->rollback_parent();
        else f->rollback_to(std::stoi(version_str));
        f->recent_priority=++timestamp_counter;
        f->priority=f->recent_priority; recent_heap.add_or_update(f);
    }

    void read_file(const std::string &fname){
        if(!files.contains(fname)){ std::cout<<"Error: File '"<<fname<<"' not found."<<std::endl; return;}
        files.get(fname)->read_print();
    }

    void history_file(const std::string &fname){
        if(!files.contains(fname)){ std::cout<<"Error: File '"<<fname<<"' not found."<<std::endl; return;}
        files.get(fname)->history_cmd();
    }

    void delete_file(const std::string &fname){
        if(!files.contains(fname)){ std::cout<<"Error: File '"<<fname<<"' not found."<<std::endl; return;}
        FileVersionTree* f=files.get(fname);
        recent_heap.remove(fname);
        biggest_heap.remove(fname);
        files.remove(fname);
        delete f;
    }

    void list_files(){
        auto items = files.items();
        for(auto &p:items){
            std::cout<<p.first<<" ("<<p.second->getTotalVersions()<<" versions)"<<std::endl;
        }
    }

    void recent_files(int n){
        auto top = recent_heap.top_n(n);
        for(auto f:top) std::cout<<f->getName()<<" ("<<f->getLastModified()<<")\n";
    }

    void biggest_trees(int n){
        auto top = biggest_heap.top_n(n);
        for(auto f:top) std::cout<<f->getName()<<" ("<<f->getTotalVersions()<<" versions)\n";
    }
};

//======================== MAIN =================================
int main() {
    FileSystem fs;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "CREATE") {
            std::string fname;
            ss >> fname;
            if (fname.empty()) { std::cout << "Error: Invalid command. Usage: CREATE <filename>\n"; continue; }
            if (fs.files.contains(fname)) {
                std::cout << "Error: File '" << fname << "' already exists.\n";
            } else {
                fs.create_file(fname);
                std::cout << "File '" << fname << "' created with snapshot version 0.\n";
            }
        } 
        else if (cmd == "INSERT") {
            std::string fname, content;
            ss >> fname;
            std::getline(ss, content);
            if (fname.empty() || content.empty()) { std::cout << "Error: Invalid command. Usage: INSERT <filename> <content>\n"; continue;}
            content.erase(0, content.find_first_not_of(" \"")); // trim leading spaces/quotes
            content.erase(content.find_last_not_of("\"") + 1); // trim trailing quotes
            if (!fs.files.contains(fname)) { std::cout << "Error: File '" << fname << "' not found.\n"; continue; }
            fs.insert_into(fname, content);
        } 
        else if (cmd == "UPDATE") {
            std::string fname, content;
            ss >> fname;
            std::getline(ss, content);
            if (fname.empty() || content.empty()) { std::cout << "Error: Invalid command. Usage: UPDATE <filename> <content>\n"; continue;}
            content.erase(0, content.find_first_not_of(" \"")); // trim spaces/quotes
            content.erase(content.find_last_not_of("\"") + 1);
            if (!fs.files.contains(fname)) { std::cout << "Error: File '" << fname << "' not found.\n"; continue; }
            fs.update_file(fname, content);
        } 
        else if (cmd == "SNAPSHOT") {
            std::string fname, message;
            ss >> fname;
            std::getline(ss, message);
            if (fname.empty() || message.empty()) { std::cout << "Error: Invalid command. Usage: SNAPSHOT <filename> <message>\n"; continue;}
            message.erase(0, message.find_first_not_of(" \"")); // trim spaces/quotes
            message.erase(message.find_last_not_of("\"") + 1);
            if (!fs.files.contains(fname)) { std::cout << "Error: File '" << fname << "' not found.\n"; continue; }
            fs.snapshot_file(fname, message);
        } 
        else if (cmd == "ROLLBACK") {
            std::string fname, version_str;
            ss >> fname >> version_str;
            if (fname.empty()) { std::cout << "Error: Invalid command. Usage: ROLLBACK <filename> [versionID]\n"; continue;}
            if (!fs.files.contains(fname)) { std::cout << "Error: File '" << fname << "' not found.\n"; continue; }
            if (version_str.empty()) fs.rollback_file(fname, "");
            else fs.rollback_file(fname, version_str);
        } 
        else if (cmd == "READ") {
            std::string fname;
            ss >> fname;
            if (fname.empty()) { std::cout << "Error: Invalid command. Usage: READ <filename>\n"; continue; }
            fs.read_file(fname);
        } 
        else if (cmd == "HISTORY") {
            std::string fname;
            ss >> fname;
            if (fname.empty()) { std::cout << "Error: Invalid command. Usage: HISTORY <filename>\n"; continue; }
            fs.history_file(fname);
        } 
        else if (cmd == "BIGGEST_TREES") {
            int n;
            ss >> n;
            if (n <= 0) n = 3;
            fs.biggest_trees(n);
        } 
        else if (cmd == "RECENT_FILES") {
            int n;
            if (!(ss >> n)) n = 3; 
            fs.recent_files(n);
        } 
        else {
            std::cout << "Error: Unknown command '" << cmd << "'.\n";
        }
    }

    return 0;
}
