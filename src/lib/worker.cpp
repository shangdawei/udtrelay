#include <time.h>
#include <utils.h>
#include <logger.h>
#include <socket_api.h>
#include <loopbuffer.h>

extern Logger logger;

int utl::worker (int fd_in, int fd_out, int sd, timeval * timeout) {

    const int R_ACTIVE   =  1 << 0;
    const int W_ACTIVE   =  1 << 1;
    const int R_FWD_SHUT =  1 << 2;
    const int W_FWD_SHUT =  1 << 3;
    const int R_REV_SHUT =  1 << 4;
    const int W_REV_SHUT =  1 << 5;
    
    int status = R_ACTIVE | W_ACTIVE;

    int   blsize = 32*1024;
    int   bfsize = 32*blsize;
    
    Loopbuffer fwd_buf(bfsize,blsize);
    Loopbuffer rev_buf(bfsize,blsize);

    
    SOCK_API::FDSET rset;
    SOCK_API::FDSET wset;
    SOCK_API::FDSET eset;

    bool  true_bool = true;
    bool  false_bool = false;
    
    if(SOCK_API::setsockopt(fd_in, 0,  UDT_SNDSYN, &false_bool, sizeof(bool)))
        logger.log_die("worker: setsockopt(... UDT_SNDSYN ...): error");
    if(SOCK_API::setsockopt(fd_out, 0, UDT_SNDSYN, &false_bool, sizeof(bool)))
        logger.log_die("worker: setsockopt(... UDT_SNDSYN ...): error");
    if(SOCK_API::setsockopt(sd, 0,     UDT_SNDSYN, &false_bool, sizeof(bool)))
        logger.log_die("worker: setsockopt(... UDT_SNDSYN ...): error");
    
    logger.log_debug(2, "start worker\n");

    char str[globals::dump_message*5+10];

    int maxfdn = SOCK_API::maxfdn(fd_out, sd);

    time_t start;
    time_t stamp;
    
    start = stamp = time(0);

    timeval thp, thz;
    
    thz.tv_sec  =  0;
    thz.tv_usec =  0;
    
    thp.tv_sec  =  0;
    thp.tv_usec = 10;
    
    while ( (!(status & W_FWD_SHUT) or !(status & W_REV_SHUT))
            and (timeout == NULL or time(0) - stamp < timeout->tv_sec)) {

        logger.log_debug(3,"  Status = %d\n", status);

        timeval to;       

        to.tv_sec = 0;
        to.tv_usec = 500000;
        
        ::select(0,NULL,NULL,NULL,&to);
        
        rset.ZERO();
        wset.ZERO();
        eset.ZERO();
        
        rset.SET(fd_in);
        rset.SET(sd);

        eset.SET(fd_in);
        eset.SET(fd_out);
        eset.SET(sd);

        //logger.log_debug(3," rd select (active)... \n");
        to = status & R_ACTIVE ? thz : thp;
        if(SOCK_API::select(maxfdn, &rset, NULL, &eset, &to) > 0)
            status |= R_ACTIVE;
        else
            status &= ~ R_ACTIVE;
        //logger.log_debug(3," ... rd select (active)\n");
        
       
        if (rset.ISSET(fd_in)) {
            wset.SET(sd);
        }
        if (rset.ISSET(sd)) {
            wset.SET(fd_out);
        }
        
        eset.SET(fd_in);
        eset.SET(fd_out);
        eset.SET(sd);
        
        to = thz;
        
        //logger.log_debug(3," wr select ... \n");
        
        to = status & W_ACTIVE ? thz : thp;
        if (SOCK_API::select(maxfdn, NULL, &wset, &eset, &to) > 0)
            status |= W_ACTIVE;
        else
            status &= (~W_ACTIVE);
        
        //logger.log_debug(3," ... wr select \n");


        // fd_in -> sd
        int sz;
        status &= ~(R_ACTIVE | W_ACTIVE);

        if (rset.ISSET(fd_in) and !fwd_buf.isful() and !(status & R_FWD_SHUT)) {
            //logger.log_debug(3," read ... \n");
            int n = fwd_buf.getWriteSpace();
            sz = SOCK_API::read(fd_in, 
                    fwd_buf.getWritePointer(), 
                    n);
            //logger.log_debug(3," ... read \n");
            if (sz > 0) {
                fwd_buf.writen(sz);
                logger.log_debug(3,"  sd1 -> sd2 read %d/%d bytes: [%s]\n", n, sz,
                             utl::dump_str(str, fwd_buf.getReadPointer(), n, sz, globals::dump_message));
            }
            else if (sz == 0) {
                //logger.log_debug(3,"  sd1 -> sd2 EOF\n");
                //status |= R_FWD_SHUT;
            }
            else {
                logger.log_debug(3,"  sd1 -> sd2 read error\n");
                status |= (R_FWD_SHUT | W_FWD_SHUT);
            }
        }
        if (wset.ISSET(sd) and !fwd_buf.isempty() and !(status & W_FWD_SHUT)) {
            sz = SOCK_API::write(sd, 
                    fwd_buf.getReadPointer(),
                    fwd_buf.getReadSpace());
            if (sz>0) {
                logger.log_debug(3,"  sd1 -> sd2 write %d/%d bytes: [%s]\n", fwd_buf.getReadSpace(), sz,
                             utl::dump_str(str, fwd_buf.getReadPointer(), fwd_buf.getReadSpace(), sz, globals::dump_message));
                fwd_buf.readn(sz);
            }
            else {
                logger.log_debug(3,"  sd1 -> sd2 write error (%d)\n", sz);
                status |= (W_FWD_SHUT | R_FWD_SHUT);
            }
        }
        if (fwd_buf.isempty() and (status & R_FWD_SHUT)) {
            logger.log_debug("  W_FWD_SHUT status = %d\n", status);
            status |= W_FWD_SHUT;
        }
            
        if (rset.ISSET(sd) and !rev_buf.isful() and !(status & R_REV_SHUT)) {
            //logger.log_debug(3," read ... \n");
            int n = rev_buf.getWriteSpace();
            sz = SOCK_API::read(fd_in, 
                    rev_buf.getWritePointer(), 
                    n);
            //logger.log_debug(3," ... read \n");
            if (sz > 0) {
                logger.log_debug(3,"  sd2 -> sd1 read %d/%d bytes: [%s]\n", n, sz,
                             utl::dump_str(str, rev_buf.getWritePointer(), n, sz, globals::dump_message));
                rev_buf.writen(sz);
            }
            else if (sz == 0) {
                logger.log_debug(3,"  sd2 -> sd1 EOF\n");
                status |= R_REV_SHUT;
            }
            else {
                logger.log_debug(3,"  sd2 -> sd1 read error\n");
                status |= (R_REV_SHUT | W_REV_SHUT);
            }
        }
        if (wset.ISSET(fd_out) and !rev_buf.isempty() and !(status & W_REV_SHUT)) {
            int n = rev_buf.getReadSpace();
            sz = SOCK_API::write(sd, 
                    rev_buf.getReadPointer(),
                    n);
            if (sz>0) {
                logger.log_debug(3,"  sd2 -> sd1 write %d/%d bytes: [%s]\n", rev_buf.getReadSpace(), sz,
                             utl::dump_str(str, rev_buf.getReadPointer(), n, sz, globals::dump_message));
                rev_buf.readn(sz);
            }
            else {
                logger.log_debug(3,"  sd2 -> sd1 write error (%d)\n", sz);
                status |= (W_REV_SHUT | R_REV_SHUT);
            }
        }
        if (rev_buf.isempty() and (status & R_REV_SHUT)) {
            logger.log_debug("  W_REV_SHUT status = %d\n", status);
            status |= W_REV_SHUT;
        }
    }
    SOCK_API::close(fd_in);
    if (fd_in != fd_out)
        SOCK_API::close(fd_out);
    SOCK_API::close(sd);

    //SOCK_API::shutdown(sd, SHUT_RD);
    //SOCK_API::shutdown(fd_out, SHUT_WR);

    logger.log_debug(2, "stop worker\n");
    return 0;
}

int utl::worker (int sd1, int sd2, timeval * to) {
    return utl::worker (sd1, sd1, sd2);
}
