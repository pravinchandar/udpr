#define WRITE_MODE 0
#define READ_MODE 1

/* The circular buffer structure */
typedef struct {
	void* elems;       /* The start of the block returned by malloc */
	int elem_size;     /* The size of each blob */
	int num_elems;     /* The total number of blobs being stored */ 
	void* w_ptr;       /* The address of the blob being written */
	void* r_ptr;       /* The address of the blob being read */
	void* l_ptr;       /* The address of the last blob */
	int written;
	int read;
} cb;

/* Alloc memory for the circular buffer */
void cb_init(cb* c_buff, int elem_size, int num_elems);

/* Dealloc memory assigned for the circular buffer */
void cb_destroy(cb* c_buff);

/* Write a blob into the circular buffer */
void cb_write_elem(cb* c_buff, void* elem); 

/* Read a blob from the circular buffer */
void cb_read_elem(cb* c_buff, void* elem);

/* Update the read pointer on the circular buffer */
void cb_move_rptr(cb* c_buff);

/* Update the write pointer on the circular buffer */
static void cb_move_wptr(cb* c_buff);
