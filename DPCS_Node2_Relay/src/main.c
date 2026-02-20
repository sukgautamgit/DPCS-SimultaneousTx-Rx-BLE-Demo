/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>


#define ADV_PAYLOAD_LEN 8
int count_termination=0;  


#define TIMEOUT_SYNC_CREATE K_SECONDS(20)
static bool         per_adv_found;
static bt_addr_le_t per_addr;
static uint8_t      per_sid;
static K_SEM_DEFINE(sem_per_adv, 0, 1);
static K_SEM_DEFINE(sem_per_sync, 0, 1);
static K_SEM_DEFINE(sem_per_sync_lost, 0, 1);

/* Scan parameters */
struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_PASSIVE,
		.options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE|
					  BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST,  /* Filter Accept List enables whitelisting */
		.interval   = 0x00A0, //100 ms
		.window     = 0x00A0,
	};

/* Creating an advertising instance */
struct bt_le_ext_adv *adv;

static uint8_t adv_array[ADV_PAYLOAD_LEN-2]= {0}; 

static const struct bt_data ad[] = {BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_array, ADV_PAYLOAD_LEN-2),}; 


static void set_random_static_address(void)
{
	int err;       
	bt_addr_le_t addr;
	err=bt_addr_le_from_str("D2:F0:F4:22:53:28","random" , &addr); /* Random Static Address assigned to this node */
                if (err) {
	printk("Invalid BT address (err %d)\n", err);
                         }
	/* Create a new Identity address for this node. This must be done 
	* "before enabling Bluetooth" (before executing bt_enable). */
	err=bt_id_create(&addr, NULL);
                if (err < 0) {
	printk("Creating new ID failed (err %d)\n", err);
	}
    printk("Created new address\n");
}


static const char *phy2str(uint8_t phy)
{
	switch (phy) {
	case BT_GAP_LE_PHY_NONE: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}


#if defined(CONFIG_BT_EXT_ADV)

static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
		printk("Primary channel reception alert..\n");


		if (!per_adv_found && info->interval) {
		per_adv_found = true;

		per_sid = info->sid;
		bt_addr_le_copy(&per_addr, info->addr);

		k_sem_give(&sem_per_adv);
		   }
}


/* Scan callback functions */
static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,  /* Primary channel packet reception */
};
#endif /* CONFIG_BT_EXT_ADV */


/*Start periodic advertising*/
void start_adv()
{
	
	struct bt_le_adv_param adv_param= BT_LE_ADV_PARAM_INIT(
									  BT_LE_ADV_OPT_NONE|
									  BT_LE_ADV_OPT_EXT_ADV|
									  BT_LE_ADV_OPT_USE_IDENTITY|
									  BT_LE_ADV_OPT_NO_2M,
                                      0x320, 0x320, NULL);  //advertising interval= 500 ms.
														
    
struct bt_le_ext_adv_start_param ext_adv_param= BT_LE_EXT_ADV_START_PARAM_INIT(0,0); 

/* Create a non-connectable non-scannable advertising set */
    int err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
    if (err) {
        printk("Failed to create advertising set (err %d)\n", err);
        return;
    }

/* Set periodic advertising parameters */
   struct bt_le_per_adv_param periodic_param= BT_LE_PER_ADV_PARAM_INIT(
											  0x320, 0x320, 
											  BT_LE_PER_ADV_OPT_NONE); //periodic advertising interval= 1 second.

    err = bt_le_per_adv_set_param(adv, &periodic_param);
    if (err) {
        printk("Failed to set periodic advertising parameters (err %d)\n", err);
        return;
    }

    /* Set the data to be transmitted inside periodic advertising events */
    printk("Set Periodic Advertising Data...");
            err = bt_le_per_adv_set_data(adv, ad, ARRAY_SIZE(ad));
            if (err) {
                printk("Failed (err %d)\n", err);
                return;
            }
            printk("done.\n"); 

	/* Enable Periodic Advertising */
    err = bt_le_per_adv_start(adv);
    if (err) {
        printk("Failed to enable periodic advertising (err %d)\n", err);
        return;
    }

    /* Enable Extended Advertising */
    printk("Start Extended Advertising...");
    err = bt_le_ext_adv_start(adv, &ext_adv_param);
        if (err) {
            printk("Failed to start extended advertising (err %d)\n", err);
            return;
        }
        printk("done.\n");
}



#if defined(CONFIG_BT_PER_ADV_SYNC)
static void sync_cb(struct bt_le_per_adv_sync *sync,
		    struct bt_le_per_adv_sync_synced_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	printk("PER_ADV_SYNC[%u]: [DEVICE]: %s synced, "
	       "Interval 0x%04x (%u ms), PHY %s\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr,
	       info->interval, info->interval * 5 / 4, phy2str(info->phy));
	
	/* Stop primary channel scanning once periodic synchronization is established
 	* with the previous node. 
	*/	
	int err=bt_le_scan_stop();
		if(err==0)
		{
			printk("Stopped Scanning..!!\n");
		}

	/* After periodic synchronization is established, start this node's own periodic
 	* advertising (enables simultaneous RX/TX for relay operation).
 	* This should only be done once (when count_termination == 0), and not upon every
 	* subsequent sync establishment within the same power cycle.
 	*/	
	if(count_termination==0)
	{
	start_adv();
	}
	
	k_sem_give(&sem_per_sync);
	
}


static void term_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	printk("PER_ADV_SYNC[%u]: [DEVICE]: %s sync terminated\n",
	       bt_le_per_adv_sync_get_index(sync), le_addr);


	/* Re-enable primary channel scanning if sync is terminated. */
	int err = bt_le_scan_start(&scan_param, NULL);
	if (err) {
		printk("Start scanning failed (err %d)\n", err);
		return;
	}
	
	count_termination++;  //keeps a count of sync termination instances.
	printk("Resumed primary channel scanning...\n");
	printk("Reason behind termination: %d\n",info->reason);
	k_sem_give(&sem_per_sync_lost);

}


/* Access the data received in periodic advertising event*/
static void recv_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info,
		    struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char data_str[200];

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	bin2hex(buf->data, buf->len, data_str, sizeof(data_str));

		   printk("Periodic Advertising Data Received: ");

		   for (int i=0;i<buf->len;i=i+1)
		   {
			printk("%x ",buf->data[i]);
		   }
		
			/* 2 bytes of Company ID to be included in the MAnufacturer Specific AD Structure */ 
			adv_array[0] = 0x59;
			adv_array[1] = 0x00;

		   /* Copy only the "data" of the received buffer into the new buffer, excluding 4 bytes 
		   * of Manufacturer Specific AD structure header */
			memcpy(&adv_array[2], &buf->data[4], ADV_PAYLOAD_LEN - 4);  
			printk("\nSet Periodic Advertising Data...");
            int err = bt_le_per_adv_set_data(adv, ad, ARRAY_SIZE(ad));
            if (err) {
                printk("Failed (err %d)\n", err);
                return;
            }
            printk("done.\n");

			/* Print the number of times synchronization was lost 
			* in this power cycle */
			printk("Sync lost count: %d\n",count_termination);

}


static struct bt_le_per_adv_sync_cb sync_callbacks = {
	.synced = sync_cb,  //periodic advertising sync established.
	.term = term_cb,    //periodic advertising sync terminated.
	.recv = recv_cb     //periodic advertising event received.
};

#endif



int main(void)
{

    int err;

    set_random_static_address();

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

	/* Random Static Address of the previous node (to be whitelisted) */
	bt_addr_le_t addr1;

	bt_addr_le_from_str("DE:AD:BE:AF:BA:11","random" , &addr1);
	
	/* Add the address of the previous node to the whitelist (Filter Accept List) */
	err=bt_le_filter_accept_list_add(&addr1);
	if (err)
	{
		printk("Whitelist not updated (err %d)\n", err);
	}
	else
	{
		printk("Node-1 successfully whitelisted.\n");
	}
	
	
	/* Callbacks associated with primary advertising channel "scanning" activity. */
	#if defined(CONFIG_BT_EXT_ADV)
		printk("Scan callbacks register...");
		bt_le_scan_cb_register(&scan_callbacks);
		printk("Registered scan callbacks\n");
	#endif /* CONFIG_BT_EXT_ADV */

	
	#if defined(CONFIG_BT_PER_ADV_SYNC)
		struct bt_le_per_adv_sync_param sync_create_param;
		struct bt_le_per_adv_sync *sync;
		printk("Periodic Advertising callbacks register...");
		bt_le_per_adv_sync_cb_register(&sync_callbacks); //Callbacks associated with periodic synchronization.
		printk("Success.\n");	
	#endif


	err = bt_le_scan_start(&scan_param, NULL);  //Start primary channel scanning
	if (err) {
		printk("Start scanning failed (err %d)\n", err);
		return 0;
	}
	printk("Started scanning...\n");


	#if defined(CONFIG_BT_PER_ADV_SYNC)
	do{
		printk("Waiting for periodic advertiser...\n");
		per_adv_found = false;
		err = k_sem_take(&sem_per_adv, K_FOREVER);
		if (err) {
			printk("failed (err %d)\n", err);
			return 0;
		}
		printk("Found periodic advertising.\n");

		printk("Creating Periodic Advertising Sync... \n");
		bt_addr_le_copy(&sync_create_param.addr, &per_addr);

		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(&sync_create_param.addr, addr_str, sizeof(addr_str));
		printk("Address %s\n", addr_str);

		sync_create_param.options = 0;
		sync_create_param.sid = per_sid;
		sync_create_param.skip = 0;
		sync_create_param.timeout = 0x1F4; /* sync timeout set to 5 seconds */
										   /* maximum allowed value= 0x4000 equivalent to 163.84 s */
		err = bt_le_per_adv_sync_create(&sync_create_param, &sync);
		if (err) {
			printk("Periodic sync creation failed (err %d)\n", err);
			return 0;
		}
		printk("Periodic sync creation started.\n");

		printk("Waiting for periodic sync...\n");
		err = k_sem_take(&sem_per_sync, TIMEOUT_SYNC_CREATE);
		if (err) {
			printk("failed (err %d)\n", err);

			printk("Deleting Periodic Advertising Sync...");
			err = bt_le_per_adv_sync_delete(sync);
			if (err) {
				printk("failed (err %d)\n", err);
				return 0;
			}
			continue;
		}
		printk("Periodic sync established.\n");

		printk("Waiting for periodic sync lost...\n");
		err = k_sem_take(&sem_per_sync_lost, K_FOREVER);
		if (err) {
			printk("failed (err %d)\n", err);
			return 0;
		}
		printk("Periodic sync lost.\n");
	} while (true);
	#endif
}