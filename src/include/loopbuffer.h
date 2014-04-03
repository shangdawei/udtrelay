#ifndef _LOOPBUFFER_H_
#define _LOOPBUFFER_H_

class Loopbuffer {
    int len;  // buffer lenght
    int atom; //  
    int size; // data size
    char * buf;
    
    int read_p; // pointer to begin
    int writ_p; // pointer to end
    
public:
    Loopbuffer (int sz, int at=0) 
    : len(sz), size(0), read_p(0), writ_p(0), atom(at) 
    {
        buf = new char[sz];
        if (atom = 0)
            atom = len;
    }
    ~Loopbuffer () {
        delete [] buf;
    }
    inline bool isful() {
        return (size == len);
    }
    inline bool isempty() {
        return (size == 0);
    }
    inline int getReadSpace() {
        return std::min(atom,
                read_p + size <= len ? size : len - read_p);
    }
    inline int getWriteSpace() {
        if (size == len)
            return 0;
        return std::min(atom,
                writ_p > read_p ? len - writ_p : read_p - writ_p);
    }
    inline char * getReadPointer() {
        return buf+read_p;
    }
    inline char * getWritePointer() {
        return buf+writ_p;
    }
    inline void writen(int n) {
        assert(writ_p+n <= len && n <= len-size);
        writ_p = (writ_p+n) % len;
        size += n;
    }
    inline void readn(int n) {
        assert(read_p+n <=len && n <= size);
        read_p = (read_p+n) % len;
        size -= n;
    }
};
#endif
