#include <linux/kernel.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/rmi.h>

#define COMMS_DEBUG 0

#define IRQ_DEBUG 0

#define SMB_PROTOCOL_VERSION_ADDRESS	0xfd
#define RMI_PAGE_SELECT_REGISTER 0xff
#define RMI_SMB_PAGE(addr) (((addr) >> 8) & 0xff)
#define SMB_MAX_COUNT      32
#define RMI_SMB2_MAP_SIZE      8  /* 8 entry of 4 bytes each */
#define RMI_SMB2_MAP_FLAGS_WE      0x01

static char *smb_v1_proto_name = "smb1";   /* smbus old version */
static char *smb_v2_proto_name = "smb2";   /* smbus new version */

struct mapping_table_entry {
	union {
		struct {
			u16 rmiaddr;
			u8 readcount;
			u8 flags;
		};
		u8 entry[4];
	};
};


struct rmi_smb_data {
	struct mutex page_mutex;
	int page;
	int enabled;
	int irq;
	int irq_flags;
	struct rmi_phys_device *phys;
	u8 table_index;
	struct mutex mappingtable_mutex;
	struct mapping_table_entry mapping_table[RMI_SMB2_MAP_SIZE];
};

static irqreturn_t rmi_smb_irq_thread(int irq, void *p)
{
	struct rmi_phys_device *phys = p;
	struct rmi_device *rmi_dev = phys->rmi_dev;
	struct rmi_driver *driver = rmi_dev->driver;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;

#if IRQ_DEBUG
	dev_dbg(phys->dev, "ATTN gpio, value: %d.\n",
			gpio_get_value(pdata->attn_gpio));
#endif
	if (gpio_get_value(pdata->attn_gpio) == pdata->attn_polarity) {
		phys->info.attn_count++;
		if (driver && driver->irq_handler && rmi_dev)
			driver->irq_handler(rmi_dev, irq);
	}

	return IRQ_HANDLED;
}

/*
 * rmi_set_page - Set RMI page
 * @phys: The pointer to the rmi_phys_device struct
 * @page: The new page address.
 *
 * RMI devices have 16-bit addressing, but some of the physical
 * implementations (like SMBus) only have 8-bit addressing. So RMI implements
 * a page address at 0xff of every page so we can reliable page addresses
 * every 256 registers.
 *
 * The page_mutex lock must be held when this function is entered.
 *
 * Returns zero on success, non-zero on failure.
 */
static int rmi_set_page(struct rmi_phys_device *phys, unsigned int page)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	char txbuf[2] = {RMI_PAGE_SELECT_REGISTER, page};
	int retval;

#if COMMS_DEBUG
	dev_dbg(&client->dev, "RMI4 SMB writes 3 bytes: %02x %02x\n",
		txbuf[0], txbuf[1]);
#endif
	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval != sizeof(txbuf)) {
		phys->info.tx_errs++;
		dev_err(&client->dev,
			"%s: set page failed: %d.", __func__, retval);
		return (retval < 0) ? retval : -EIO;
	}
	data->page = page;
	return 0;
}

static int rmi_smb_v1_write_block(struct rmi_phys_device *phys, u16 addr,
				  u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	u8 txbuf[len + 1];
	int retval;
#if	COMMS_DEBUG
	int i;
#endif

	txbuf[0] = addr & 0xff;
	memcpy(txbuf + 1, buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_SMB_PAGE(addr) != data->page) {
		retval = rmi_set_page(phys, RMI_SMB_PAGE(addr));
		if (retval < 0)
			goto exit;
	}

#if COMMS_DEBUG
	dev_dbg(&client->dev, "RMI4 SMB v1 writes %d bytes: ", sizeof(txbuf));
	for (i = 0; i < sizeof(txbuf); i++)
		dev_dbg(&client->dev, "%02x ", txbuf[i]);
	dev_dbg(&client->dev, "\n");
#endif

	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval < 0)
		phys->info.tx_errs++;

exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

static int rmi_smb_v1_write(struct rmi_phys_device *phys, u16 addr, u8 data)
{
	int retval = rmi_smb_v1_write_block(phys, addr, &data, 1);
	return (retval < 0) ? retval : 0;
}

static int rmi_smb_v1_read_block(struct rmi_phys_device *phys, u16 addr,
			u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	u8 txbuf[1] = {addr & 0xff};
	int retval;
#if	COMMS_DEBUG
	int i;
#endif

	mutex_lock(&data->page_mutex);

	if (RMI_SMB_PAGE(addr) != data->page) {
		retval = rmi_set_page(phys, RMI_SMB_PAGE(addr));
		if (retval < 0)
			goto exit;
	}

#if COMMS_DEBUG
	dev_dbg(&client->dev, "RMI4 SMB writes 1 bytes: %02x\n", txbuf[0]);
#endif
	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);
	retval = i2c_master_send(client, txbuf, sizeof(txbuf));
	if (retval != sizeof(txbuf)) {
		phys->info.tx_errs++;
		retval = (retval < 0) ? retval : -EIO;
		goto exit;
	}

	retval = i2c_master_recv(client, buf, len);

	phys->info.rx_count++;
	phys->info.rx_bytes += len;
	if (retval < 0)
		phys->info.rx_errs++;
#if COMMS_DEBUG
	else {
		dev_dbg(&client->dev, "RMI4 SMB v1 received %d bytes: ", len);
		for (i = 0; i < len; i++)
			dev_dbg(&client->dev, "%02x ", buf[i]);
		dev_dbg(&client->dev, "\n");
	}
#endif

exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

static int rmi_smb_v1_read(struct rmi_phys_device *phys, u16 addr, u8 *buf)
{
	int retval = rmi_smb_v1_read_block(phys, addr, buf, 1);
	return (retval < 0) ? retval : 0;
}

/*SMB version 2 block write - wrapper over ic2_smb_write_block */
static int smb_v2_block_write(struct rmi_phys_device *phys,
			u8 commandcode, u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	u8 txbuf[len + 1];
	int retval;
#if	COMMS_DEBUG
	int i;
#endif

	txbuf[0] = commandcode & 0xff;
	memcpy(txbuf + 1, buf, len);

#if COMMS_DEBUG
	dev_dbg(&client->dev, "RMI4 SMB v2 writes %d bytes: ", sizeof(txbuf));
	for (i = 0; i < sizeof(txbuf); i++)
		dev_dbg(&client->dev, "%02x ", txbuf[i]);
	dev_dbg(&client->dev, "\n");
#endif


	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);

	retval = i2c_smbus_write_block_data(client, commandcode, len, txbuf);

	if (retval < 0)
		phys->info.tx_errs++;
	return retval;
}

/* The function to get command code for smbus operations and keeps
records to the driver mapping table */
static int rmi_smb_v2_get_command_code(struct rmi_phys_device *phys,
		u16 rmiaddr, int bytecount, bool isread, u8 *commandcode)
{
	struct rmi_smb_data *data = phys->data;
	int i;
	int retval;
	struct mapping_table_entry *mapping_data;
	mutex_lock(&data->mappingtable_mutex);
	for (i = 0; i < RMI_SMB2_MAP_SIZE; i++) {
		if (data->mapping_table[i].rmiaddr == rmiaddr) {
			if (isread) {
				if (data->mapping_table[i].readcount
							== bytecount) {
					*commandcode = i;
					return 0;
				}
			} else {
				if (data->mapping_table[i].flags &
							RMI_SMB2_MAP_FLAGS_WE) {
					*commandcode = i;
					return 0;
				}
			}
		}
	}
	i = data->table_index;
	data->table_index = (i + 1) % RMI_SMB2_MAP_SIZE;

	mapping_data = kzalloc(sizeof(struct mapping_table_entry), GFP_KERNEL);
	if (!mapping_data) {
		retval = -ENOMEM;
		goto exit;
	}
	/* constructs mapping table data entry. 4 bytes each entry */
	mapping_data->rmiaddr = rmiaddr;
	mapping_data->readcount = bytecount;
	mapping_data->flags = RMI_SMB2_MAP_FLAGS_WE; /* enable write */

	retval = smb_v2_block_write(phys, i+0x80, mapping_data,
				    sizeof(mapping_data));
	if (retval < 0) {
		/* if not written to device mapping table */
		/* clear the driver mapping table records */
		data->mapping_table[i].rmiaddr = 0x0000;
		data->mapping_table[i].readcount = 0;
		data->mapping_table[i].flags = 0;
		goto exit;
	}
	/* save to the driver level mapping table */
	data->mapping_table[i].rmiaddr = rmiaddr;
	data->mapping_table[i].readcount = bytecount;
	data->mapping_table[i].flags = RMI_SMB2_MAP_FLAGS_WE;
	*commandcode = i;

exit:
	mutex_unlock(&data->mappingtable_mutex);
	return retval;
}

static int rmi_smb_v2_write_block(struct rmi_phys_device *phys, u16 rmiaddr,
		u8 *databuff, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	int retval = 0;
	u8 commandcode;
	struct rmi_smb_data *data = phys->data;

	mutex_lock(&data->page_mutex);

	while (len > 0) {  /* while more than 32 bytes */
		/* break into 32 butes chunks to write */
		/* get command code */
		int block_len = min(len, SMB_MAX_COUNT);
		retval = rmi_smb_v2_get_command_code(phys, rmiaddr, block_len,
			false, &commandcode);
		if (retval < 0)
			goto exit;
		/* write to smb device */
		retval = smb_v2_block_write(phys, commandcode,
			databuff, block_len);
		if (retval < 0)
			goto exit;

		/* prepare to write next block of bytes */
		len -= SMB_MAX_COUNT;
		databuff += SMB_MAX_COUNT;
		rmiaddr += SMB_MAX_COUNT;
	}
exit:
	mutex_unlock(&data->page_mutex);
	return retval;
}

static int rmi_smb_v2_write(struct rmi_phys_device *phys, u16 addr, u8 data)
{
	int retval = rmi_smb_v2_write_block(phys, addr, &data, 1);
	return (retval < 0) ? retval : 0;
}


/*SMB version 2 block read - wrapper over ic2_smb_read_block */
static int smb_v2_block_read(struct rmi_phys_device *phys,
			u8 commandcode, u8 *buf, int len)
{
	struct i2c_client *client = to_i2c_client(phys->dev);
	struct rmi_smb_data *data = phys->data;
	u8 txbuf[len + 1];
	txbuf[0] = commandcode;
	int retval;
#if	COMMS_DEBUG
	int i;
#endif
/*	mutex_lock(&data->page_mutex); */

#if COMMS_DEBUG
	dev_dbg(&client->dev, "RMI4 SMB reads 1 bytes: %02x\n", txbuf[0]);
#endif
	phys->info.tx_count++;
	phys->info.tx_bytes += sizeof(txbuf);

	retval = i2c_smbus_read_block_data(client, commandcode, buf);
	phys->info.rx_count++;
	phys->info.rx_bytes += len;
	if (retval < 0) {
		phys->info.rx_errs++;
		return retval;
	}
#if COMMS_DEBUG
	else {
		dev_dbg(&client->dev, "RMI4 SMB v2 received %d bytes: ", len);
		for (i = 0; i < len; i++)
			dev_dbg(&client->dev, "%02x ", buf[i]);
		dev_dbg(&client->dev, "\n");
	}
#endif

exit:
/*	mutex_unlock(&data->page_mutex);	*/
	return retval;
}

static int rmi_smb_v2_read_block(struct rmi_phys_device *phys, u16 rmiaddr,
					u8 *databuff, int len)
{
	struct rmi_smb_data *data = phys->data;
	int retval;
#if	COMMS_DEBUG
	int i;
#endif
	u8 commandcode;

	mutex_lock(&data->page_mutex);

	while (len > 0) {
		/* break into 32 bytes chunks to write */
		/* get command code */
		int block_len = min(len, SMB_MAX_COUNT);

		retval = rmi_smb_v2_get_command_code(phys, rmiaddr, block_len,
			false, &commandcode);
		if (retval < 0)
			goto exit;
		/* read to smb device */
		retval = smb_v2_block_read(phys, commandcode,
			databuff, block_len);
		if (retval < 0)
			goto exit;

		/* prepare to read next block of bytes */
		len -= SMB_MAX_COUNT;
		databuff += SMB_MAX_COUNT;
		rmiaddr += SMB_MAX_COUNT;
	}

exit:
	mutex_unlock(&data->page_mutex);
	return 0;
}

static int rmi_smb_v2_read(struct rmi_phys_device *phys, u16 addr, u8 *buf)
{
	int retval = rmi_smb_v2_read_block(phys, addr, buf, 1);
	return (retval < 0) ? retval : 0;
}

static int acquire_attn_irq(struct rmi_smb_data *data)
{
	return request_threaded_irq(data->irq, NULL, rmi_smb_irq_thread,
			data->irq_flags, dev_name(data->phys->dev), data->phys);
}

static int enable_device(struct rmi_phys_device *phys)
{
	int retval = 0;

	struct rmi_smb_data *data = phys->data;

	if (data->enabled)
		return 0;

	retval = acquire_attn_irq(data);
	if (retval)
		goto error_exit;

	data->enabled = true;
	dev_dbg(phys->dev, "Physical device enabled.\n");
	return 0;

error_exit:
	dev_err(phys->dev, "Failed to enable physical device. Code=%d.\n",
		retval);
	return retval;
}


static void disable_device(struct rmi_phys_device *phys)
{
	struct rmi_smb_data *data = phys->data;

	if (!data->enabled)
		return;

	disable_irq(data->irq);
	free_irq(data->irq, data->phys);

	dev_dbg(phys->dev, "Physical device disabled.\n");
	data->enabled = false;
}


static int __devinit rmi_smb_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	char dbg_buf[50];
	struct rmi_phys_device *rmi_phys;
	struct rmi_smb_data *data;
	struct rmi_device_platform_data *pdata = client->dev.platform_data;
	int retval;
	int smbus_version;
	if (!pdata) {
		dev_err(&client->dev, "no platform data\n");
		return -EINVAL;
	}
	pr_info("%s: Probing %s (IRQ %d).\n", __func__,
		pdata->sensor_name ? pdata->sensor_name : "-no name-",
		pdata->attn_gpio);

	retval = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!retval) {
		dev_err(&client->dev, "i2c_check_functionality error %d.\n",
				retval);
		return retval;
	}

	rmi_phys = kzalloc(sizeof(struct rmi_phys_device), GFP_KERNEL);
	if (!rmi_phys)
		return -ENOMEM;

	data = kzalloc(sizeof(struct rmi_smb_data), GFP_KERNEL);
	if (!data) {
		retval = -ENOMEM;
		goto err_phys;
	}

	data->enabled = true;	/* We plan to come up enabled. */
	data->irq = gpio_to_irq(pdata->attn_gpio);
	data->irq_flags = (pdata->attn_polarity == RMI_ATTN_ACTIVE_HIGH) ?
		IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
	data->phys = rmi_phys;

	rmi_phys->data = data;
	rmi_phys->dev = &client->dev;
	rmi_phys->enable_device = enable_device;
	rmi_phys->disable_device = disable_device;

	mutex_init(&data->page_mutex);
	mutex_init(&data->mappingtable_mutex);

	if (pdata->gpio_config) {
		retval = pdata->gpio_config(pdata->gpio_data, true);
		if (retval < 0) {
			dev_err(&client->dev, "failed to setup irq %d\n",
				pdata->attn_gpio);
			goto err_data;
		}
	}

	/* Check if for SMBus new version device by reading version byte. */
	retval = i2c_smbus_read_byte_data(client, SMB_PROTOCOL_VERSION_ADDRESS);
	if (retval < 0) {
		dev_err(&client->dev, "failed to get SMBus version number!\n");
		goto err_data;
	}
	smbus_version = retval + 1;
	dev_dbg(&client->dev, "Smbus version is %d", smbus_version);
	switch (smbus_version) {
	case 1:
		/* Setting the page to zero will (a) make sure the PSR is in a
		* known state, and (b) make sure we can talk to the device. */
		retval = rmi_set_page(rmi_phys, 0);
		if (retval) {
			dev_err(&client->dev, "Failed to set page select to 0.\n");
			goto err_data;
		}
		rmi_phys->write = rmi_smb_v1_write;
		rmi_phys->write_block = rmi_smb_v1_write_block;
		rmi_phys->read = rmi_smb_v1_read;
		rmi_phys->read_block = rmi_smb_v1_read_block;
		rmi_phys->info.proto = smb_v1_proto_name;
		break;
	case 2:
		/* SMBv2 */
		retval = i2c_check_functionality(client->adapter,
						I2C_FUNC_SMBUS_READ_BLOCK_DATA);
		if (retval < 0) {
			dev_err(&client->dev, "client's adapter does not support the I2C_FUNC_SMBUS_READ_BLOCK_DATA functionality.\n");
			goto err_data;
		}

		rmi_phys->write		= rmi_smb_v2_write;
		rmi_phys->write_block	= rmi_smb_v2_write_block;
		rmi_phys->read		= rmi_smb_v2_read;
		rmi_phys->read_block	= rmi_smb_v2_read_block;
		rmi_phys->info.proto	= smb_v2_proto_name;
		break;
	default:
		dev_err(&client->dev, "Unrecognized SMB version %d.\n",
				smbus_version);
		retval = -ENODEV;
		goto err_data;
	}
	/* End check if this is an SMBus device */

	retval = rmi_register_phys_device(rmi_phys);
	if (retval) {
		dev_err(&client->dev, "failed to register physical driver at 0x%.2X.\n",
			client->addr);
		goto err_data;
	}
	i2c_set_clientdata(client, rmi_phys);

	if (pdata->attn_gpio > 0) {
		retval = acquire_attn_irq(data);
		if (retval < 0) {
			dev_err(&client->dev,
				"request_threaded_irq failed %d\n",
				pdata->attn_gpio);
			goto err_unregister;
		}
	}

#if defined(CONFIG_RMI4_DEV)
	retval = gpio_export(pdata->attn_gpio, false);
	if (retval) {
		dev_warn(&client->dev, "%s: WARNING: Failed to "
				 "export ATTN gpio!\n", __func__);
		retval = 0;
	} else {
		retval = gpio_export_link(&(rmi_phys->rmi_dev->dev), "attn",
					pdata->attn_gpio);
		if (retval) {
			dev_warn(&(rmi_phys->rmi_dev->dev), "%s: WARNING: "
				"Failed to symlink ATTN gpio!\n", __func__);
			retval = 0;
		} else {
			dev_info(&(rmi_phys->rmi_dev->dev),
				"%s: Exported GPIO %d.", __func__,
				pdata->attn_gpio);
		}
	}
#endif /* CONFIG_RMI4_DEV */

	dev_info(&client->dev, "registered rmi smb driver at 0x%.2X.\n",
			client->addr);
	return 0;

err_unregister:
	rmi_unregister_phys_device(rmi_phys);
err_data:
	kfree(data);
err_phys:
	kfree(rmi_phys);
	return retval;
}

static int __devexit rmi_smb_remove(struct i2c_client *client)
{
	struct rmi_phys_device *phys = i2c_get_clientdata(client);
	struct rmi_device_platform_data *pd = client->dev.platform_data;

	rmi_unregister_phys_device(phys);
	kfree(phys->data);
	kfree(phys);

	if (pd->gpio_config)
		pd->gpio_config(&client->dev, false);

	return 0;
}

static const struct i2c_device_id rmi_id[] = {
	{ "rmi", 0 },
	{ "rmi-smb", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rmi_id);

static struct i2c_driver rmi_smb_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "rmi-smb"
	},
	.id_table	= rmi_id,
	.probe		= rmi_smb_probe,
	.remove		= __devexit_p(rmi_smb_remove),
};

static int __init rmi_smb_init(void)
{
	return i2c_add_driver(&rmi_smb_driver);
}

static void __exit rmi_smb_exit(void)
{
	i2c_del_driver(&rmi_smb_driver);
}

MODULE_AUTHOR("Allie Xiong <axiong@synaptics.com>");
MODULE_DESCRIPTION("RMI SMBus driver");
MODULE_LICENSE("GPL");

module_init(rmi_smb_init);
module_exit(rmi_smb_exit);
