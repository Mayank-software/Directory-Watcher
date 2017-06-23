#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <map>
#include <unistd.h>

using namespace std;


#define EVENT_SIZE          ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN       ( 1024 * ( EVENT_SIZE + NAME_MAX + 1) )
#define WATCH_FLAGS         ( IN_CREATE | IN_DELETE  )

// Keep going  while run == true, or, in other words, until user hits ctrl-c
static bool run = true;

void sig_callback( int sig )
{
    run = false;
}

class Watch {
    struct wd_elem {
        int pd;
        string name;
        bool operator() (const wd_elem &l, const wd_elem &r) const
            { return l.pd < r.pd ? true : l.pd == r.pd && l.name < r.name ? true : false; }
    };
    map<int, wd_elem> watch;
    map<wd_elem, int, wd_elem> rwatch;
public:
    // Insert event information, used to create new watch, into Watch object.
    void insert( int pd, const string &name, int wd ) {
        wd_elem elem = {pd, name};
        watch[wd] = elem;
        rwatch[elem] = wd;
    }
    // Erase watch specified by pd (parent watch descriptor) and name from watch list.
    // Returns full name (for display etc), and wd, which is required for inotify_rm_watch.
    string erase( int pd, const string &name, int *wd ) {
        wd_elem pelem = {pd, name};
        *wd = rwatch[pelem];
        rwatch.erase(pelem);
        const wd_elem &elem = watch[*wd];
        string dir = elem.name;
        watch.erase(*wd);
        return dir;
    }
    // Given a watch descriptor, return the full directory name as string. Recurses up parent WDs to assemble name, 
    // an idea borrowed from Windows change journals.
    string get( int wd ) {
        const wd_elem &elem = watch[wd];
        return elem.pd == -1 ? elem.name : this->get(elem.pd) + "/" + elem.name;
    }
    // Given a parent wd and name (provided in IN_DELETE events), return the watch descriptor.
    // Main purpose is to help remove directories from watch list.
    int get( int pd, string name ) {
        wd_elem elem = {pd, name};
        return rwatch[elem];
    }
    void cleanup( int fd ) {
        for (map<int, wd_elem>::iterator wi = watch.begin(); wi != watch.end(); wi++) {
            inotify_rm_watch( fd, wi->first );
            watch.erase(wi);
        }
        rwatch.clear();
    }
    void stats() {
        cout << "number of watches=" << watch.size() << " & reverse watches=" << rwatch.size() << endl;
    }
};

int main( )
{    
    cout << "Startc1";
    Watch watch;
    cout << "Start";
    
    fd_set watch_set;

    char buffer[ EVENT_BUF_LEN ];
    string current_dir, new_dir;
    int total_file_events = 0;
    int total_dir_events = 0;

    // Call sig_callback if user hits ctrl-c
    signal( SIGINT, sig_callback );


#ifdef IN_NONBLOCK
    int fd = inotify_init1( IN_NONBLOCK );
#else
    int fd = inotify_init();
#endif

    // checking for error
    if ( fd < 0 ) {
        perror( "inotify_init" );
    }

    // use select watch list for non-blocking inotify read
    FD_ZERO( &watch_set );
    FD_SET( fd, &watch_set );

    // add ./tmp to watch list. Normally, should check directory exists first
    const char *root = "//home//chromotifuser//Desktop";
    int wd = inotify_add_watch( fd, root, WATCH_FLAGS );
    
    // add wd and directory name to Watch map
    watch.insert( -1, root, wd );

    // Continue until run == false. See signal and sig_callback above.
    while ( run ) {
        // select waits until inotify has 1 or more events.
        // select syntax is beyond the scope of this sample but, don't worry, the fd+1 is correct:
        // select needs the the highest fd (+1) as the first parameter.
        select( fd+1, &watch_set, NULL, NULL, NULL );

        // Read event(s) from non-blocking inotify fd (non-blocking specified in inotify_init1 above).
        int length = read( fd, buffer, EVENT_BUF_LEN ); 
        if ( length < 0 ) {
            perror( "read" );
        }  

        // Loop through event buffer
        for ( int i=0; i<length; ) {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
            // Never actually seen this
            if ( event->wd == -1 ) {
   	        printf( "Overflow\n" );
			}
            // Never seen this either		
            if ( event->mask & IN_Q_OVERFLOW ) {
	      		printf( "Overflow\n" );
    	    }
            if ( event->len ) {
                if ( event->mask & IN_IGNORED ) {
                    printf( "IN_IGNORED\n" );
                }
                if ( event->mask & IN_CREATE ) {
                    current_dir = watch.get(event->wd);
                    if ( event->mask & IN_ISDIR ) {
                	    new_dir = current_dir + "/" + event->name;
                	    wd = inotify_add_watch( fd, new_dir.c_str(), WATCH_FLAGS );
                	    watch.insert( event->wd, event->name, wd );
                        total_dir_events++;
                        printf( "New directory %s created.\n", new_dir.c_str() );
                    } else {
                        total_file_events++;
                        printf( "New file %s/%s created.\n", current_dir.c_str(), event->name );
                    }
                } else if ( event->mask & IN_DELETE ) {
                    if ( event->mask & IN_ISDIR ) {
                        new_dir = watch.erase( event->wd, event->name, &wd );
                        inotify_rm_watch( fd, wd );
                        total_dir_events--;
                        printf( "Directory %s deleted.\n", new_dir.c_str() );
                    } else {
                        current_dir = watch.get(event->wd);
                        total_file_events--;
                        printf( "File %s/%s deleted.\n", current_dir.c_str(), event->name );
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    // Cleanup
    printf( "cleaning up\n" );
    cout << "total dir events = " << total_dir_events << ", total file events = " << total_file_events << endl;
    watch.stats();
    watch.cleanup( fd );
    watch.stats();
    //close( fd );
    fflush(stdout); 
}