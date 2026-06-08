// Public surface of the offline Max stub layer (see max_stub.c).
#ifndef KARMA_MAX_STUB_H
#define KARMA_MAX_STUB_H

typedef struct {
    float *data;
    long   frames;   // frames per channel
    long   chans;    // channels (interleaved)
    double sr;       // sample rate
    int    valid;
} mock_buffer;

// Install the backing buffer~ that buffer_* calls will report/return.
void         mock_buffer_install(float *data, long frames, long chans, double sr);
mock_buffer *mock_buffer_get(void);

// Capture of the most recent outlet_list() emission (the data/report outlet).
void   mock_outlet_reset(void);
long   mock_outlet_count(void);   // -1 if nothing emitted since reset
double mock_outlet_value(long i);

#endif
