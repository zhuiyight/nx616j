/*
 * Touchkey driver for CYPRESS4000 controller
 *
 * Copyright (C) 2018 nubia
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c/mcs.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/reboot.h>
#include <linux/kernel.h>
#include "cy8c4014l_touchkey.h"
#include "cy8c4014l_touchkey_StringImage.h"
#include "cy8c4014l_touchkey_firmware_update.h"
static ssize_t touchkey_command_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    if (sscanf(buf, "%u", &input) != 1)
    {
        dev_info(dev, "Failed to get input message!\n");
        return -EINVAL;
    }
    /* 0x1 enter read cp mode
        0x2 soft reset IC ,other command are not supported for now*/
    if(input == 0x1 || input == 0x2)
    {
        ret = i2c_smbus_write_byte_data(client,CYPRESS4000_TOUCHKEY_CMD, input);
        if(ret<0)
        {
            dev_err(&client->dev, "Failed to set command 0x%x!\n",input);
        }
    }
    else
    {
        dev_info(dev, "Command 0x%x not support!\n",input);
    }
    return count;
}
static ssize_t touchkey_command_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int cmd = 0;
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    cmd = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_CMD);
    if(cmd < 0)
    {
        dev_info(dev, "Failed to get command state(0x%x)!\n",cmd);
        return scnprintf(buf,CYPRESS_BUFF_LENGTH, "cmd: error\n");
    }
    return scnprintf(buf,CYPRESS_BUFF_LENGTH, "cmd: 0x%x\n", cmd);
}

static ssize_t touchkey_mode_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int mode = 0;
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    mode = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_MODE);
    //dev_info(dev, "touchkey_mode_show [0x%x]\n", mode);
    return scnprintf(buf,CYPRESS_BUFF_LENGTH, "mode: %s(0x%x)\n",
        (mode == CYPRESS4000_WAKEUP_MODE)?"wakeup":((mode == CYPRESS4000_SLEEP_MODE)?"sleep":"unknown"),
        mode);
}

static ssize_t touchkey_firmversion_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int version = 0;
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    version = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_FW);
    //dev_info(dev, "touchkey_firmversion_show [0x%x]\n", version);
    return scnprintf(buf,CYPRESS_BUFF_LENGTH, "firmware version: 0x%x\n", version);
}
static ssize_t touchkey_reglist_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u8 regval = 0;
    u8 regaddr = CYPRESS4000_KEY0_RAWDATA0;
    int size = 0;
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);

    for(regaddr = CYPRESS4000_KEY0_RAWDATA0;regaddr <= CYPRESS4000_KEY1_CP3;regaddr++)
    {
        regval = i2c_smbus_read_byte_data(client, regaddr);
        size += scnprintf(buf + size , CYPRESS_BUFF_LENGTH - size, "Reg[0x%x] :%d\n",regaddr,regval);
        //dev_info(dev, "size=%d,reg[0x%x]=%d\n", size,regaddr,regval);
    }
    return size;
}
static ssize_t touchkey_signal_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u8 raw0 = 0;
    u8 raw1 = 0;
    u8 base0 = 0;
    u8 base1 = 0;
    int size = 0;

    struct i2c_client *client = container_of(dev, struct i2c_client, dev);

    raw0 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY0_RAWDATA0);
    raw1 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY0_RAWDATA1);
    base0 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY0_BASELINE0);
    base1 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY0_BASELINE1);
    size += scnprintf(buf + size, CYPRESS_BUFF_LENGTH - size,
        "[Key0 signal %d]:%d %d %d %d\n",
        (((raw0 << 8) | raw1) - ((base0 << 8) | base1)),raw0,raw1,base0,base1);


    raw0 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY1_RAWDATA0);
    raw1 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY1_RAWDATA1);
    base0 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY1_BASELINE0);
    base1 = i2c_smbus_read_byte_data(client, CYPRESS4000_KEY1_BASELINE1);
    size += scnprintf(buf + size, CYPRESS_BUFF_LENGTH - size,
        "[Key1 signal %d]:%d %d %d %d\n",
        (((raw0 << 8) | raw1) - ((base0 << 8) | base1)),raw0,raw1,base0,base1);

    return size;
}
static struct device_attribute attrs[] = {
    __ATTR(mode, S_IRUGO|S_IXUGO,touchkey_mode_show, NULL),
    __ATTR(firm_version, S_IRUGO|S_IXUGO,touchkey_firmversion_show, NULL),
    __ATTR(reg_list, S_IRUGO|S_IXUGO,touchkey_reglist_show, NULL),
    __ATTR(key_signal, S_IRUGO|S_IXUGO,touchkey_signal_show, NULL),
    __ATTR(command, S_IRUGO|S_IXUGO|S_IWUSR|S_IWGRP,touchkey_command_show, touchkey_command_store)
};

static int cypress_power_on(bool val)
{
    int ret = 0;
    if (val)
    {
    // add power on func
    }
    else
    {
    //add power off func
    }
    return ret;
}

static irqreturn_t cypress_touchkey_interrupt(int irq, void *dev_id)
{

    struct cypress_info * info = dev_id;
    struct i2c_client *client = info->i2c;
    struct input_dev *input = info->platform_data->input_dev;
    u8 val;
    int cur_value;
    int status;
    static int last_value = 0;

    //read key value single keys value are 0x01, 0x02, both pressed keys value is 0x03
    val = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_KEYVAL);
    if (val < 0) {
        dev_err(&client->dev, "cypress key read error [%d]\n", val);
        goto out;
    }
    dev_info(&client->dev, "val = 0x%x\n", val);

    cur_value = (val == 0xff ) ? 0x0 : val;
    status = last_value ^ cur_value;
    if(status & CYPRESS_LEFT_BIT)
    {
        input_report_key(input, CYPRESS_KEY_LEFT, (cur_value & CYPRESS_LEFT_BIT)?1:0);
        input_sync(input);
    }
    if(status & CYPRESS_RIGHT_BIT)
    {
        input_report_key(input, CYPRESS_KEY_RIGHT, (cur_value & CYPRESS_RIGHT_BIT)?1:0);
        input_sync(input);
    }
    last_value = cur_value;
 out:
    return IRQ_HANDLED;
}
static int parse_dt(struct device *dev, struct cypress_platform_data *pdata)
{
    int retval;
    u32 value;
    struct device_node *np = dev->of_node;

    pdata->irq_gpio = of_get_named_gpio_flags(np,
        "touchkey,irq-gpio", 0, NULL);

    retval = of_property_read_u32(np, "touchkey,irq-flags", &value);
    if (retval < 0)
        return retval;
    else
        pdata->irq_flag = value;

    return 0;
}

static int cypress_touchkey_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct cypress_info *info = NULL;
    struct cypress_platform_data *pdata = client->dev.platform_data;
    struct input_dev *input_dev = NULL;
    int ret = 0;
    int fw_ver;
    int attr_count = 0;

    dev_info(&client->dev, "Cypress probe start!\n");
    cypress_power_on(true);
    info = kzalloc(sizeof(struct cypress_info), GFP_KERNEL);
    if (!info)
    {
        dev_err(&client->dev, "kzalloc failed!\n");
        ret= -ENOMEM;
        goto info_err;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) 
    {
        dev_err(&client->dev, "i2c_check_functionality error!\n");
        ret = -ENODEV;
        goto info_err;
    }

    if (client->dev.of_node)
    {
        pdata = devm_kzalloc(&client->dev,
            sizeof(struct cypress_platform_data),
            GFP_KERNEL);
        if (!pdata)
        {
            dev_err(&client->dev, "Failed to allocate memory\n");
            ret = -ENOMEM;
            goto info_err;
        }

        client->dev.platform_data = pdata;
        ret = parse_dt(&client->dev,pdata);
        if (ret)
        {
            dev_err(&client->dev, "Cypress parse device tree error\n");
            goto data_err;
        }
        else
        {
            dev_info(&client->dev, "Cypress irq gpio:%d,irg flag:0x%x\n",pdata->irq_gpio,pdata->irq_flag);
        }
    }
    else
    {
        pdata = client->dev.platform_data;
        if (!pdata)
        {
            dev_err(&client->dev, "No platform data\n");
            ret = -ENODEV;
            goto info_err;
        }
    }

    input_dev = input_allocate_device();
    if(input_dev == NULL)
    {
       dev_info(&client->dev, "Failed to allocate input device !\n");
       ret= -ENOMEM;
       goto info_err;
    }
    input_dev->name = "cypress_touchkey";
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &client->dev;
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(CYPRESS_KEY_LEFT, input_dev->keybit);
    __set_bit(CYPRESS_KEY_RIGHT, input_dev->keybit);

    ret = input_register_device(input_dev);
    if (ret)
    {
        dev_info(&client->dev, "Failed to register input device !\n");
        goto input_err;
    }

    pdata->input_dev = input_dev;
    info->i2c = client;
    info->platform_data = pdata;

    info->irq = gpio_to_irq(pdata->irq_gpio);
    ret = gpio_direction_input(pdata->irq_gpio);
    if(ret)
    {
        dev_err(&client->dev, "Failed to set gpio\n");
        goto data_err;
    }
    i2c_set_clientdata(client, info);

    //read fw version and update
    fw_ver = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_FW);
    dev_err(&client->dev, "current fw version:[0x%x] , new fw version [0x%x]\n", 
    fw_ver,CURRENT_NEW_FIRMWARE_VER);
    if (fw_ver != CURRENT_NEW_FIRMWARE_VER)
    {
        int ret; //0:success
        dev_info(&client->dev, "Ready to update firmware\n");
        ret = cypress_firmware_update(client,stringImage_0, LINE_CNT_0);
        if (ret != 0)
            dev_err(&client->dev, "cypress Firmware update fail,cur ver is :%x,ret=%x\n", fw_ver,ret);
        else
            dev_err(&client->dev, "cypress Firmware update success\n");
    }

    ret = request_threaded_irq(info->irq, NULL, cypress_touchkey_interrupt,
        pdata->irq_flag,client->dev.driver->name, info);
    if (ret)
    {
        dev_err(&client->dev, "Failed to register interrupt\n");
        goto irq_err;
    }

    for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
    {
        ret = sysfs_create_file(&client->dev.kobj,&attrs[attr_count].attr);
        if (ret < 0)
        {
            dev_err(&client->dev,"%s: Failed to create sysfs attributes\n",__func__);
        }
    }

    return 0;

irq_err:
    free_irq(info->irq, info);
data_err:
    //devm_kfree(pdata);
input_err:
    input_free_device(input_dev);
info_err:
    kfree(info);
    return ret;
}


static int cypress_touchkey_remove(struct i2c_client *client)
{
#if 0
	struct cypress_touchkey_data *data = i2c_get_clientdata(client);
    int attr_count = 0;
	free_irq(client->irq, data);
	cypress_power_on(false);
	input_unregister_device(data->input_dev);
	kfree(data);
    for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
    {
        sysfs_remove_file(&client->dev.kobj,&attrs[attr_count].attr);
    }
#endif
	return 0;
}

static void cypress_touchkey_shutdown(struct i2c_client *client)
{

    cypress_power_on(false);
}

#ifdef CONFIG_PM_SLEEP
static int cypress_touchkey_suspend(struct device *dev)
{
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    int ret;
    ret = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_MODE);
    dev_err(&client->dev, "suspend: current mode = 0x%x\n",ret);

    ret = i2c_smbus_write_byte_data(client,CYPRESS4000_TOUCHKEY_MODE, CYPRESS4000_SLEEP_MODE);
    if(ret<0)
    {
        dev_err(&client->dev, "Set sleep mode error!\n");
    }
    return 0;
}

static int cypress_touchkey_resume(struct device *dev)
{
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    int ret;
    ret = i2c_smbus_read_byte_data(client, CYPRESS4000_TOUCHKEY_MODE);
    dev_err(&client->dev, "resume: current mode = 0x%x\n",ret);
    ret = i2c_smbus_write_byte_data(client,CYPRESS4000_TOUCHKEY_MODE, CYPRESS4000_WAKEUP_MODE);
    if(ret<0)
    {
        dev_err(&client->dev, "Set wakeup mode error!\n");
    }
    return 0;
}
#endif


static SIMPLE_DEV_PM_OPS(cypress_touchkey_pm_ops,
    cypress_touchkey_suspend, cypress_touchkey_resume);

static const struct i2c_device_id cypress_touchkey_id[] = {
    { "cypress,touchkey", 0 },
    {},
};
MODULE_DEVICE_TABLE(i2c, cypress_touchkey_id);


static struct of_device_id cypress_touchkey_match_table[] = {
        { .compatible = "cypress,touchkey-i2c",},
        {},
};

static struct i2c_driver cypress_touchkey_driver = {
    .driver = {
        .name = "cypress_touchkey",
        .owner = THIS_MODULE,
        .of_match_table = cypress_touchkey_match_table,
        .pm = &cypress_touchkey_pm_ops,
    },
    .probe = cypress_touchkey_probe,
    .remove = cypress_touchkey_remove,
    .shutdown = cypress_touchkey_shutdown,
    .id_table = cypress_touchkey_id,
};

module_i2c_driver(cypress_touchkey_driver);

/* Module information */
MODULE_AUTHOR("nubia, Inc.");
MODULE_DESCRIPTION("Touchkey driver for cy8c4014lqi-421");
MODULE_LICENSE("GPL");
