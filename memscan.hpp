#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

// Value width we scan/write. Byte is for flags, Float for things
// like a fuel/nitrous bar, Int for counters and currency.
enum class ValType { Byte, Int, Float };

// How a filter pass compares the current value to the previous one.
enum class Filter { Changed, Unchanged, Increased, Decreased };

struct Hit { uintptr_t address; double value; };

class MemScanner {
public:
    ~MemScanner() { stopFreeze(); if (proc) CloseHandle(proc); }

    bool attach(const std::string& exeName) {
        DWORD pid = pidByName(exeName);
        if (!pid) return false;
        proc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE |
                           PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
                           FALSE, pid);
        return proc != nullptr;
    }

    // First pass when the target number is known (e.g. cash).
    void exactScan(ValType t, double target) {
        type = t; hits.clear(); snaps.clear(); haveList = true;
        int w = width(t);
        walkRegions([&](uintptr_t base, const uint8_t* buf, size_t n) {
            for (size_t i = 0; i + w <= n; i += w)
                if (read(buf + i, t) == target)
                    hits.push_back({ base + i, target });
        });
    }

    // First pass when the number is unknown. Grabs a full snapshot;
    // the next filter pass compares against it.
    void snapshot(ValType t) {
        type = t; hits.clear(); snaps.clear(); haveList = false;
        walkRegions([&](uintptr_t base, const uint8_t* buf, size_t n) {
            snaps.push_back({ base, std::vector<uint8_t>(buf, buf + n) });
        });
    }

    // Narrow the candidates by how the value behaved.
    void filter(Filter f) {
        int w = width(type);
        std::vector<Hit> keep;

        if (haveList) {
            uint8_t tmp[8];
            for (auto& h : hits) {
                SIZE_T got = 0;
                if (ReadProcessMemory(proc, (LPCVOID)h.address, tmp, w, &got) && got == (SIZE_T)w) {
                    double now = read(tmp, type);
                    if (match(now, h.value, f)) keep.push_back({ h.address, now });
                }
            }
        } else {
            for (auto& s : snaps) {
                std::vector<uint8_t> live(s.bytes.size());
                SIZE_T got = 0;
                if (!ReadProcessMemory(proc, (LPCVOID)s.base, live.data(), s.bytes.size(), &got)) continue;
                size_t n = got < s.bytes.size() ? got : s.bytes.size();
                for (size_t off = 0; off + w <= n; off += w) {
                    double before = read(&s.bytes[off], type);
                    double now    = read(&live[off], type);
                    if (match(now, before, f)) keep.push_back({ s.base + off, now });
                }
            }
            snaps.clear();
            haveList = true;
        }
        hits.swap(keep);
    }

    bool write(uintptr_t address, double value, ValType t) {
        SIZE_T out = 0;
        if (t == ValType::Byte)  { BYTE  b = (BYTE)(int)value; return WriteProcessMemory(proc, (LPVOID)address, &b, 1, &out) && out == 1; }
        if (t == ValType::Int)   { int   v = (int)value;       return WriteProcessMemory(proc, (LPVOID)address, &v, 4, &out) && out == 4; }
        float g = (float)value;                                 return WriteProcessMemory(proc, (LPVOID)address, &g, 4, &out) && out == 4;
    }

    // Hold a set of addresses at fixed values on a background thread.
    void freeze(const std::vector<Hit>& targets, ValType t) {
        { std::lock_guard<std::mutex> lk(mtx); for (auto& h : targets) frozen.push_back({ h, t }); }
        if (!running.exchange(true)) worker = std::thread([this]{ freezeLoop(); });
    }

    void stopFreeze() {
        running = false;
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> lk(mtx);
        frozen.clear();
    }

    const std::vector<Hit>& results() const { return hits; }

private:
    struct Snap   { uintptr_t base; std::vector<uint8_t> bytes; };
    struct Frozen { Hit hit; ValType type; };

    HANDLE proc = nullptr;
    ValType type = ValType::Int;
    bool haveList = false;
    std::vector<Hit>  hits;
    std::vector<Snap> snaps;

    std::vector<Frozen> frozen;
    std::mutex mtx;
    std::atomic<bool> running{ false };
    std::thread worker;

    static int width(ValType t) { return t == ValType::Byte ? 1 : 4; }

    static double read(const uint8_t* p, ValType t) {
        if (t == ValType::Byte)  { uint8_t v; std::memcpy(&v, p, 1); return v; }
        if (t == ValType::Int)   { int32_t v; std::memcpy(&v, p, 4); return v; }
        float v; std::memcpy(&v, p, 4); return v;
    }

    static bool match(double now, double before, Filter f) {
        switch (f) {
            case Filter::Changed:   return now != before;
            case Filter::Unchanged: return now == before;
            case Filter::Increased: return now >  before;
            case Filter::Decreased: return now <  before;
        }
        return false;
    }

    static DWORD pidByName(const std::string& name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32 e{}; e.dwSize = sizeof(e);
        DWORD pid = 0;
        if (Process32First(snap, &e))
            do { if (_stricmp(e.szExeFile, name.c_str()) == 0) { pid = e.th32ProcessID; break; } }
            while (Process32Next(snap, &e));
        CloseHandle(snap);
        return pid;
    }

    // Only committed, writable pages — that's where game state lives.
    template <class Fn>
    void walkRegions(Fn onRegion) {
        MEMORY_BASIC_INFORMATION mbi{};
        uintptr_t addr = 0;
        DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY |
                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        while (VirtualQueryEx(proc, (LPCVOID)addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            bool ok = mbi.State == MEM_COMMIT
                   && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
                   && (mbi.Protect & writable);
            if (ok) {
                std::vector<uint8_t> buf(mbi.RegionSize);
                SIZE_T got = 0;
                if (ReadProcessMemory(proc, mbi.BaseAddress, buf.data(), mbi.RegionSize, &got) && got)
                    onRegion((uintptr_t)mbi.BaseAddress, buf.data(), got);
            }
            addr += mbi.RegionSize;
        }
    }

    void freezeLoop() {
        while (running.load()) {
            { std::lock_guard<std::mutex> lk(mtx);
              for (auto& f : frozen) write(f.hit.address, f.hit.value, f.type); }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};
