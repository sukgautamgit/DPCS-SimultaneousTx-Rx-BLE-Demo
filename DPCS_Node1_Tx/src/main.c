/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <string.h>

#define ADV_PAYLOAD_LEN 8   /* [Length (1) + AD Type (1) + CompanyID(2) + Application Data(4)] */

static struct bt_le_ext_adv *adv;

static uint8_t adv_array[ADV_PAYLOAD_LEN-2] = { 0x59, 0x00, 0x00, 0x01, 0x02, 0x03};

/* Manufacturer data length = Company ID (2B) + app data (4B) = 6B */
static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_array, ADV_PAYLOAD_LEN - 2),
};

static void set_random_static_address(void)
{
	int err;
	bt_addr_le_t addr;

	err = bt_addr_le_from_str("DE:AD:BE:AF:BA:11", "random", &addr); /* Random Static Address assigned to this node */
	if (err) {
		printk("Invalid BT address (err %d)\n", err);
		return;
	}

	err = bt_id_create(&addr, NULL);
	if (err < 0) {
		printk("Creating new ID failed (err %d)\n", err);
		return;
	}

	printk("Successfully created new ID\n");

}

static void start_adv(void)
{
	struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_NONE |
		BT_LE_ADV_OPT_EXT_ADV |
		BT_LE_ADV_OPT_USE_IDENTITY |
		BT_LE_ADV_OPT_NO_2M,
		0x320, 0x320, NULL);

	struct bt_le_ext_adv_start_param ext_adv_param = BT_LE_EXT_ADV_START_PARAM_INIT(0, 0);

	int err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
	if (err) {
		printk("Failed to create adv set (err %d)\n", err);
		return;
	}

	struct bt_le_per_adv_param periodic_param =
		BT_LE_PER_ADV_PARAM_INIT(0x320, 0x320, BT_LE_PER_ADV_OPT_NONE);

	err = bt_le_per_adv_set_param(adv, &periodic_param);
	if (err) {
		printk("Failed to set periodic advertising parameters (err %d)\n", err);
		return;
	}

	err = bt_le_per_adv_set_data(adv, ad, ARRAY_SIZE(ad));
	if (err) {
		printk("Failed to set periodic advertising data (err %d)\n", err);
		return;
	}

	err = bt_le_per_adv_start(adv);
	if (err) {
		printk("Failed to start periodic advertising (err %d)\n", err);
		return;
	}

	err = bt_le_ext_adv_start(adv, &ext_adv_param);
	if (err) {
		printk("Failed to start extended advertising (err %d)\n", err);
		return;
	}

	printk("Node-1 advertising started (extended + periodic)\n");
}

int main(void)
{
	int err;

	printk("Node-1 (TX only): Periodic advertiser\n");
	set_random_static_address();

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}


	

	start_adv();

	/* Update the first byte of application data every
	* periodic advertising interval (1 s) */
	while (1) {
		adv_array[2]++;
		(void)bt_le_per_adv_set_data(adv, ad, ARRAY_SIZE(ad));

		k_sleep(K_SECONDS(1));
	}
	return 0;
}