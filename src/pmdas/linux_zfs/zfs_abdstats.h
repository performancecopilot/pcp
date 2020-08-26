typedef struct zfs_abdstats {
	unsigned int struct_size;
	unsigned int linear_cnt;
	unsigned int linear_data_size;
	unsigned int scatter_cnt;
	unsigned int scatter_data_size;
	unsigned int scatter_chunk_waste;
	unsigned int scatter_order_0;
	unsigned int scatter_order_1;
	unsigned int scatter_order_2;
	unsigned int scatter_order_3;
	unsigned int scatter_order_4;
	unsigned int scatter_order_5;
	unsigned int scatter_order_6;
	unsigned int scatter_order_7;
	unsigned int scatter_order_8;
	unsigned int scatter_order_9;
	unsigned int scatter_order_10;
	unsigned int scatter_page_multi_chunk;
	unsigned int scatter_page_multi_zone;
	unsigned int scatter_page_alloc_retry;
	unsigned int scatter_sg_table_retry;
} zfs_abdstats_t;

void zfs_abdstats_fetch(zfs_abdstats_t *abdstats, regex_t *rgx_row);
