#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace fs = std::filesystem;

std::string now_iso() {
    auto t = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&tt));
    return std::string(buf);
}

std::string simple_hash_file(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    if(!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    std::hash<std::string> h;
    auto hv = h(s);
    std::ostringstream out;
    out << std::hex << std::setw(sizeof(size_t)*2) << std::setfill('0') << (hv);
    return out.str();
}

class FileTracker {
    fs::path storeDir;
    fs::path metaFile;
    std::mutex mtx;
public:
    FileTracker(const fs::path &store): storeDir(store), metaFile(store / "metadata.txt"){
        fs::create_directories(storeDir);
        if(!fs::exists(metaFile)){
            std::ofstream(metafile_path())<<"";
        }
    }
    std::string metafile_path() const { return metaFile.string(); }

    void addFile(const fs::path &src, const std::string &actor = "user", const std::string &message = ""){
        std::lock_guard lk(mtx);
        if(!fs::exists(src)) throw std::runtime_error("source not found");
        std::string hash = simple_hash_file(src);
        if(hash.empty()) throw std::runtime_error("hash failed");
        fs::path blobPath = storeDir / hash;
        if(!fs::exists(blobPath)) fs::copy_file(src, blobPath);
        auto sz = fs::file_size(blobPath);
        std::string ts = now_iso();
        // Simple file id
        std::string fileId = "file:" + hash.substr(0,12) + ":" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string versionId = "v:" + hash.substr(0,12) + ":" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        std::ofstream meta(metaFile, std::ios::app);
        if(!meta) throw std::runtime_error("cannot open metadata");
        // record: fileId|path|versionId|hash|size|ts|actor|message\n
        meta << fileId << "|" << src.string() << "|" << versionId << "|" << hash << "|" << sz << "|" << ts << "|" << actor << "|" << message << "\n";
        meta.close();
        std::cout << "Added file: " << src << " file_id=" << fileId << " blob=" << hash << "\n";
    }

    void listFiles(){
        std::lock_guard lk(mtx);
        std::ifstream meta(metaFile);
        if(!meta){ std::cout<<"No metadata\n"; return; }
        std::string line;
        while(std::getline(meta,line)){
            if(line.empty()) continue;
            // parse first two fields
            auto p1 = line.find('|');
            if(p1==std::string::npos) continue;
            auto p2 = line.find('|', p1+1);
            if(p2==std::string::npos) continue;
            std::string id = line.substr(0,p1);
            std::string path = line.substr(p1+1, p2-p1-1);
            std::cout<<"id="<<id<<" path="<<path<<"\n";
        }
    }
};

int main(int argc, char**argv){
    try{
        FileTracker t("store");
        if(argc<2){ std::cout<<"Usage: tracker add <file> | list\n"; return 0; }
        std::string cmd = argv[1];
        if(cmd=="add" && argc>=3) t.addFile(argv[2],"cli","initial add");
        else if(cmd=="list") t.listFiles();
        else std::cout<<"unknown\n";
    }catch(std::exception &e){ std::cerr<<"ERR: "<<e.what()<<"\n"; return 1; }
    return 0;
}
