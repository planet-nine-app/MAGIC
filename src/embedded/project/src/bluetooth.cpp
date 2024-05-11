#include "bluetooth.h"
#include "gpio.h"

#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/services/bas.h>
#include <stdio.h>
#include <stdint.h>
#include <zephyr/kernel.h>

// Magic BLE UUID
//
// struct BLEServices {
//     let twoWay = CBUUID(string: "5995AB90-709A-4735-AAF2-DF2C8B061BB4")
// }

// struct BLECharacteristics {
//     let readMagicGateway = CBUUID(string: "3558E2EC-BF6C-41F0-BC9F-EBB51B8C87CE")
//     let readMagicDevice = CBUUID(string: "2D98DB2F-C78D-4F15-AE30-2185CC77469A")
//     let write = CBUUID(string: "4D8D84E5-5889-4310-80BF-0D44DCB49762")
//     let notify = CBUUID(string: "CD6984D2-5055-4033-A42E-BB039FC6EF6B")
// }

namespace bluetooth
{

/* Custom Service Variables */
/*#define BT_UUID_CUSTOM_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)


    static const struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
        BT_UUID_CUSTOM_SERVICE_VAL);

    static const struct bt_uuid_128 vnd_enc_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1));

    static const struct bt_uuid_128 vnd_auth_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2));

    static const struct bt_uuid_128 vnd_long_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3));

    static const struct bt_uuid_128 vnd_write_cmd_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4));

    static const struct bt_uuid_128 vnd_signed_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5));
*/
#define BT_UUID_CUSTOM_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x5995AB90, 0x709A, 0x4735, 0xAAF2, 0xDF2C8B061BB4)

    static const struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
        BT_UUID_CUSTOM_SERVICE_VAL);


    static const struct bt_uuid_128 vnd_enc_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x3558E2EC, 0xBF6C, 0x41F0, 0xBC9F, 0xEBB51B8C87CE));

    static const struct bt_uuid_128 vnd_auth_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x4D8D84E5, 0x5889, 0x4310, 0x80BF, 0x0D44DCB49762));

    static const struct bt_uuid_128 vnd_long_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0xCD6984D2, 0x5055, 0x4033, 0xA42E, 0xBB039FC6EF6B));

    static const struct bt_uuid_128 vnd_write_cmd_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4));

    static const struct bt_uuid_128 vnd_signed_uuid = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5));

    static constexpr int VND_MAX_LEN = 20;
    static uint8_t vnd_value[VND_MAX_LEN + 1] = {'V', 'e', 'n', 'd', 'o', 'r'};
    static uint8_t vnd_auth_value[VND_MAX_LEN + 1] = {'V', 'e', 'n', 'd', 'o', 'r'};
    static uint8_t vnd_wwr_value[VND_MAX_LEN + 1] = {'V', 'e', 'n', 'd', 'o', 'r'};
    static bt_addr_le_t magic_addr;
    static struct bt_conn *magic_conn;

    const struct gpio::led_value red2 = gpio::make_led_value(1, 0, 0);
    const struct gpio::led_value green2 = gpio::make_led_value(0, 1, 0);
    const struct gpio::led_value blue2 = gpio::make_led_value(0, 0, 1);
    const struct gpio::led_value purple2 = gpio::make_led_value(1, 0, 1);


    gpio::led_value red3()
    {
        return gpio::make_led_value(1, 0, 0);
    }

    gpio::led_value green3()
    {
        return gpio::make_led_value(0, 1, 0);
    }

    gpio::led_value blue3()
    {
        return gpio::make_led_value(0, 0, 1);
    }

    gpio::led_value purple3()
    {
        return gpio::make_led_value(1, 0, 1);
    }

    gpio::led_value orange3()
    {
        return gpio::make_led_value(1, 1, 0);
    }

    gpio::led_value cyan3()
    {
        return gpio::make_led_value(0, 1, 1);
    }
    
    ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                     void *buf, uint16_t len, uint16_t offset)
    {
        const char *value = (const char *)attr->user_data;

        return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                                 strlen(value));
    }

    ssize_t write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                      const void *buf, uint16_t len, uint16_t offset,
                      uint8_t flags)
    {
        uint8_t *value = (uint8_t *)attr->user_data;

        if (offset + len > VND_MAX_LEN)
        {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
        }

        memcpy(value + offset, buf, len);
        value[offset + len] = 0;

        return len;
    }

    static uint8_t indicate_enabled;
    static uint8_t indicating;
    static struct bt_gatt_indicate_params ind_params;
    static struct bt_gatt_attr *vnd_ind_attr;

    void vnd_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
    {
        indicate_enabled = (value == BT_GATT_CCC_INDICATE) ? 1 : 0;
    }

    void indicate_cb(struct bt_conn *conn,
                     struct bt_gatt_indicate_params *params, uint8_t err)
    {
        printf("Indication %s\n", err != 0U ? "fail" : "success");
    }

    void indicate_destroy(struct bt_gatt_indicate_params *params)
    {
        printf("Indication complete\n");
        indicating = 0U;
    }

    void indicate_example()
    {
        if (indicating)
        {
            return;
        }

        ind_params.attr = vnd_ind_attr;
        ind_params.func = indicate_cb;
        ind_params.destroy = indicate_destroy;
        ind_params.data = &indicating;
        ind_params.len = sizeof(indicating);

        if (bt_gatt_indicate(NULL, &ind_params) == 0)
        {
            indicating = 1U;
        }
    }

    static constexpr int VND_LONG_MAX_LEN = 74;
    static uint8_t vnd_long_value[VND_LONG_MAX_LEN + 1] = {
        'V', 'e', 'n', 'd', 'o', 'r', ' ', 'd', 'a', 't', 'a', '1',
        'V', 'e', 'n', 'd', 'o', 'r', ' ', 'd', 'a', 't', 'a', '2',
        'V', 'e', 'n', 'd', 'o', 'r', ' ', 'd', 'a', 't', 'a', '3',
        'V', 'e', 'n', 'd', 'o', 'r', ' ', 'd', 'a', 't', 'a', '4',
        'V', 'e', 'n', 'd', 'o', 'r', ' ', 'd', 'a', 't', 'a', '5',
        'V', 'e', 'n', 'd', 'o', 'r', ' ', 'd', 'a', 't', 'a', '6',
        '.', ' '};

    ssize_t write_long_vnd(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, const void *buf,
                           uint16_t len, uint16_t offset, uint8_t flags)
    {
        uint8_t *value = (uint8_t *)attr->user_data;

        if (flags & BT_GATT_WRITE_FLAG_PREPARE)
        {
            return 0;
        }

        if (offset + len > VND_LONG_MAX_LEN)
        {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
        }

        memcpy(value + offset, buf, len);
        value[offset + len] = 0;

        return len;
    }

    static struct bt_gatt_cep vnd_long_cep = {
        .properties = BT_GATT_CEP_RELIABLE_WRITE,
    };

    static int signed_value;

    ssize_t read_signed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
    {
        const char *value = (const char *)attr->user_data;

        return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                                 sizeof(signed_value));
    }

    ssize_t write_signed(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         const void *buf, uint16_t len, uint16_t offset,
                         uint8_t flags)
    {
        uint8_t *value = (uint8_t *)attr->user_data;

        if (offset + len > sizeof(signed_value))
        {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
        }

        memcpy(value + offset, buf, len);

        return len;
    }

    ssize_t write_without_rsp_vnd(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len, uint16_t offset,
                                  uint8_t flags)
    {
        uint8_t *value = (uint8_t *)attr->user_data;

        if (!(flags & BT_GATT_WRITE_FLAG_CMD))
        {
            /* Write Request received. Reject it since this Characteristic
             * only accepts Write Without Response.
             */
            return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
        }

        if (offset + len > VND_MAX_LEN)
        {
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
        }

        memcpy(value + offset, buf, len);
        value[offset + len] = 0;

        return len;
    }

    /* Vendor Primary Service Declaration */
/*    BT_GATT_SERVICE_DEFINE(vnd_svc,
        BT_GATT_PRIMARY_SERVICE((void*)&vnd_uuid),
        BT_GATT_CHARACTERISTIC(&vnd_enc_uuid.uuid,
                                BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
                                BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                                read_vnd, write_vnd, vnd_value),
        BT_GATT_CCC(vnd_ccc_cfg_changed,
                    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),
        BT_GATT_CHARACTERISTIC(&vnd_auth_uuid.uuid,
                                BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                                BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
                                read_vnd, write_vnd, vnd_auth_value),
        BT_GATT_CHARACTERISTIC(&vnd_long_uuid.uuid, 
                                BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_EXT_PROP,
                                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE | BT_GATT_PERM_PREPARE_WRITE,
                                read_vnd, write_long_vnd, &vnd_long_value),
        BT_GATT_CEP(&vnd_long_cep),
        BT_GATT_CHARACTERISTIC(&vnd_signed_uuid.uuid, 
                                BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_AUTH,
                                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                                read_signed, write_signed, &signed_value),
        BT_GATT_CHARACTERISTIC(&vnd_write_cmd_uuid.uuid,
                                BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                BT_GATT_PERM_WRITE,
                                NULL, write_without_rsp_vnd, &vnd_wwr_value)
    );

    static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
        BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL),
    };

    void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
    {
        printf("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
    }

    static struct bt_gatt_cb gatt_callbacks = {
        .att_mtu_updated = mtu_updated};
*/
/*    void connected(struct bt_conn *conn, uint8_t error)
    {
        if (error)
        {
            gpio::set_rgb_led(1, 1, 0);
            printf("Connection failed (error 0x%02x)\n", error);
            return;
        }

        gpio::set_rgb_led(0, 1, 1);
        printf("Connected\n");
    }

    void disconnected(struct bt_conn *conn, uint8_t reason)
    {
        printf("Disconnected (reason 0x%02x)\n", reason);
    }

    BT_CONN_CB_DEFINE(conn_callbacks) = {
        .connected = connected,
        .disconnected = disconnected,
    };
*/
    void bt_ready()
    {
        printf("Bluetooth initialized\n");

        if (IS_ENABLED(CONFIG_SETTINGS))
        {
            settings_load();
        }

     /*   const auto &name = BT_LE_ADV_CONN_NAME;
        const int err = bt_le_adv_start(name, ad, ARRAY_SIZE(ad), NULL, 0);
        if (err)
        {
            printf("Advertising failed to start (err %d)\n", err);
            return;
        }*/

        gpio::display_led_values(purple3(), blue3(), blue3());
        printf("Advertising successfully started\n");
    }

    void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
    {
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printf("Passkey for %s: %06u\n", addr, passkey);
    }

    void auth_cancel(struct bt_conn *conn)
    {
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        printf("Pairing cancelled: %s\n", addr);
    }

    struct bt_conn_auth_cb auth_cb_display = {
        .passkey_display = auth_passkey_display,
        .passkey_entry = NULL,
        .cancel = auth_cancel,
    };

    void bas_notify()
    {
        uint8_t battery_level = bt_bas_get_battery_level();

        // Simulate battery level changes
        battery_level--;
        if (battery_level < 0)
        {
            battery_level = 100U;
        }

        bt_bas_set_battery_level(battery_level);
    }

        const struct bt_conn_le_create_param *param = BT_CONN_LE_CREATE_PARAM(BT_CONN_LE_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL, BT_GAP_SCAN_FAST_INTERVAL);
        const struct bt_le_conn_param *other_param = BT_LE_CONN_PARAM_DEFAULT;


    static void connect() {
        int err;

        gpio::display_led_value(blue3());
        err = bt_conn_le_create(&magic_addr, /*BT_CONN_LE_CREATE_CONN*/ param, /*BT_LE_CONN_PARAM_DEFAULT*/ other_param, &magic_conn);
        if (err) {
            printk("Create conn failed (err %d)\n", err);
        }
        //gpio::display_led_values(blue3(), green3(), blue3());
    }

/*    char * uint8_to_char(uint8_t *data, size_t size) {
	char *result = (char *)malloc(size + 1); // Allocate memory for the char array (+1 for null terminator)
	if (result == NULL) {
	    return NULL; // Allocation failed
	}
	for (size_t i = 0; i < size; i++) {
	    result[i] = (char)data[i]; // Copy uint8_t elements to char array
	}
	result[size] = '\0'; // Add null terminator
	return result;
    }
*/
    static uint8_t read_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params, const void *data, uint16_t length) {
        gpio::display_led_value(orange3());
        const uint8_t *bytes = ((uint8_t *)data);
        gpio::display_led_value(red3());
//        const std::string value = std::to_string(bytes);;
//        const std::string magic = "magic";
//        char *value = uint8_to_char(bytes, sizeof(bytes));
        //u8_to_dec(&value, sizeof(bytes) - 1, bytes);
//        char *magic = "magic";
//        gpio::display_led_values(orange3(), red3(), orange3());
/*        if (value == magic) {
            gpio::display_led_values(green3(), green3(), green3());
        } else {
            gpio::display_led_values(red3(), red3(), red3());
        }
*/	return BT_GATT_ITER_STOP;
    }

    static uint16_t magic_state_handle;

    static struct bt_gatt_read_params read_params;

    static void read_magic_state() {
        gpio::display_led_value(purple3());
        gpio::display_led_value(cyan3());
	printk("Reading current LED state\n");
	read_params.handle_count = 1;
	read_params.single.handle = magic_state_handle;
	read_params.single.offset = 0;
	read_params.func = read_func;

        gpio::display_led_value(cyan3());

	bt_gatt_read(magic_conn, &read_params);
    }

    static void write_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
    {
        if (err > 0) 
        {
            gpio::display_led_values(red3(), orange3(), red3());
            return;
        }
        gpio::display_led_values(green3(), orange3(), green3());
    }

    static void write_magic_state() {
        gpio::set_led(1);
        gpio::display_led_value(purple3());
        gpio::display_led_value(cyan3());

        static struct bt_gatt_write_params write_params;

        // Define a memory pool
	//K_MEM_POOL_DEFINE(my_pool, 1024, 1024, 1, 4);

	// String literal
	const uint8_t data[] = "do magic!";

	// Allocate memory from the memory pool
	//void *data = k_mem_pool_alloc(&my_pool, strlen(str) + 1, K_NO_WAIT);
	//if (data != NULL) {
	    // Copy string literal to the allocated memory
	  //  memcpy(data, str, strlen(str) + 1);

	    write_params.data = &data;
	//}

	write_params.handle = magic_state_handle;
	write_params.offset = 0;
	write_params.length = sizeof(data);
	write_params.func = write_func;

        gpio::display_led_values(orange3(), orange3(), orange3());
	
        bt_gatt_write(magic_conn, &write_params);

        gpio::display_led_value(green3());

        //k_free(data);
    }

    uint8_t magic_service_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *prms) {
	magic_state_handle = bt_gatt_attr_value_handle(attr);

        gpio::display_led_values(orange3(), orange3(), green3());

//        read_magic_state();
        write_magic_state();

	return BT_GATT_ITER_STOP;
    }

    static struct bt_gatt_discover_params discover_params;
        const struct bt_uuid_128 magic_write_char_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x4D8D84E5, 0x5889, 0x4310, 0x80BF, 0x0D44DCB49762));

    void discover_magic_service() 
    {
	int err;

        gpio::display_led_value(purple3());
        
        const struct bt_uuid_128 magic_read_char_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x3558E2EC, 0xBF6C, 0x41F0, 0xBC9F, 0xEBB51B8C87CE));
        

//	discover_params.uuid = &magic_read_char_uuid.uuid;
        discover_params.uuid = &magic_write_char_uuid.uuid;
	discover_params.func = magic_service_discover_cb;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

	err = bt_gatt_discover(magic_conn, &discover_params);
	if (err) {
          gpio::display_led_values(red3(), orange3(), red3());
	  printk("Discover failed(err %d)\n", err);
	  return;
	}
        gpio::display_led_values(green3(), cyan3(), green3());
    }

    static void connected(struct bt_conn *conn, uint8_t conn_err) {
	if (!conn_err) {
	  printk("Connected.\n");
          magic_conn = conn;
          gpio::display_led_values(orange3(), cyan3(), orange3());
          discover_magic_service();
	  //k_event_set(&event, EV_CONNECTED);
	} else {
	  printk("Failed to connect.\n");
	  //bt_conn_unref(gateway_conn);
	  //gateway_conn = NULL;
	}
    }

    static void disconnected(struct bt_conn *conn, uint8_t reason) {
	printk("Disconnected.\n");
	//bt_conn_unref(gateway_conn);
	//gateway_conn = NULL;
    }

    BT_CONN_CB_DEFINE(conn_callbacks) = {
	  .connected = connected,
	  .disconnected = disconnected,
    };

    static bool gateway_found_cb(struct bt_data *data, void *user_data) {
	struct bt_uuid_128 found_uuid;
	int err;

	// we are only interested in ads with a single 128-bit UUID
	if (data->data_len != BT_UUID_SIZE_128) {
          gpio::display_led_value(red3());
	  return true;
	}

        const struct bt_uuid_128 magic_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x5995AB90, 0x709A, 0x4735, 0xAAF2, 0xDF2C8B061BB4)); 


	// check if the found UUID matches
	bt_uuid_create(&found_uuid.uuid, data->data, BT_UUID_SIZE_128);

        if (found_uuid.val[0] == magic_uuid.val[0]) {
          gpio::display_led_value(green3());
        }

	if (bt_uuid_cmp(&found_uuid.uuid, &magic_uuid.uuid) != 0) {
          gpio::display_led_values(blue3(), blue3(), blue3());
	  return true;
	} else {
	  printk("Gateway service found.\n");
	memcpy(&magic_addr, user_data, BT_ADDR_LE_SIZE);
	  bt_le_scan_stop();
	//  k_event_set(&event, EV_LED_FOUND);
        // Connect
      
          gpio::display_led_values(green3(), green3(), green3());
          connect();
    
	  return false;
	}
    }

    static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
                         struct net_buf_simple *ad)
    {
        gpio::display_led_values(purple3(), green3(), purple3());

	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);
  
        bt_data_parse(ad, gateway_found_cb, (void *)addr);
    }

    bool start_scan()
    {
       /*gpio::set_rgb_led(0, 1, 1);
       k_busy_wait(5000000);
       gpio::display_led_values(red3(), red3(), red3());
       k_busy_wait(5000000); 
       gpio::display_led_values(red3(), green3(), blue3());*/
       int err;
       struct bt_le_scan_param scan_param =
       {
 	  .type = BT_LE_SCAN_TYPE_ACTIVE,
 	  .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
 	  .interval = BT_GAP_SCAN_FAST_INTERVAL,
 	  .window = BT_GAP_SCAN_FAST_WINDOW,
       };

//       gpio::display_led_values(blue3(), blue3(), blue3());
       err = bt_le_scan_start(&scan_param, device_found);

       gpio::display_led_values(purple3(), purple3(), purple3());
       if (err) {
           printf("Starting scanning failed (err %d)\n", err);
           return false;
       } 
       return true;
    }

    void purpled()
    {
        gpio::set_rgb_led(1, 0, 1);
    }

    bool initialize()
    {
        const int err = bt_enable(NULL);
        if (err)
        {
            printf("Bluetooth init failed (err %d)\n", err);
            return false;
        }

        bt_ready();

/*        if (!gpio::initialize())
        {
            printf("Error: Failed to init GPIO\n");
            return -1;
        }
*/
/*        bt_gatt_cb_register(&gatt_callbacks);

        bt_conn_auth_cb_register(&auth_cb_display);

        // Code for indications
        char str[BT_UUID_STR_LEN];
        vnd_ind_attr = bt_gatt_find_by_uuid(vnd_svc.attrs, vnd_svc.attr_count,
                                            &vnd_enc_uuid.uuid);
        bt_uuid_to_str(&vnd_enc_uuid.uuid, str, sizeof(str));
        printf("Indicate VND attr %p (UUID %s)\n", vnd_ind_attr, str);
        // End code for indications
*/
        return true;
    }
}
