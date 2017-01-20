/*
 *  mychardriver.c − Demonstrates module documentation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h> // For malloc and free
#include <asm/uaccess.h> // copy_to_user function
#include <stdbool.h>// C99!!!

#define SUCCESS 0

#define DEVICE_NAME "mychardev"
#define CLASS_NAME "mychar"
#define DRIVER_AUTHOR "Xiaobei Zhao"
#define DRIVER_DESC   "A char device driver. will map to /dev/mychardev. It can count words from the input provided by the user."

// License as GPL
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION("0.1");

static int majorNum;// Assigned dynamically by the kernel.
static int timesOpen;// How many times is this device been opened.

static struct class *  mycharClass  = NULL; ///< The device-driver class struct pointer
static struct device * mycharDevice = NULL; ///< The device-driver device struct pointer

// Declare open/release/read/write functions for the character driver
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

//==============================================================================
typedef struct wordnode
{
	char * word;
	int count;
	struct wordnode * pNext;
}WordNode;
static char * outMsg = NULL;
static int outLen = 0;
static WordNode * wordRoot = NULL;

static int getRequiredChars(WordNode * node)
{
	int outLen = 0;
	int tmp = 0;
	if(!node)
		return 0;
	outLen = strlen(node->word) + 1;// +1 for a space
	tmp = node->count;
	do// How many digits?
	{
		tmp /= 10;
		outLen++;
	}
	while(tmp > 0);

	outLen += 1;// for the '\n'!!!
	return outLen;
}

static void clearAllNodes(WordNode ** pn)
{
	WordNode * node = *pn;
	while(node)
	{
		WordNode * nodeDel = node;
		kfree(node->word);
		node = node->pNext;
		kfree(nodeDel);
	}

	*pn = NULL;
}

static bool isLetterOrNum(char c)
{
	return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
}

static void insertWordFromBuffer(WordNode ** pn, const char * pos, int len)
{
	WordNode * currNode = *pn;
	WordNode * prevNode = NULL;
	bool inserted = false;
	char * newWord = (char *)kmalloc(len + 1, GFP_KERNEL);// +1 for null terminator
	memcpy(newWord, pos, len);
	newWord[len] = 0;// null terminator

	while(currNode)
	{
		int cmpRet = strcmp(newWord, currNode->word);
		if(cmpRet == 0)
		{
			currNode->count++;
			kfree(newWord);
			inserted = true;
			break;
		}
		else if(cmpRet < 0)
		{
			break;
		}
		else if(cmpRet > 0)
		{
			prevNode = currNode;
			currNode = currNode->pNext;
		}
	}
	if(!inserted)
	{
		WordNode * newNode = (WordNode *)kmalloc(sizeof(WordNode), GFP_KERNEL);
		newNode->word = newWord;
		newNode->count = 1;
		newNode->pNext = currNode;
		if(prevNode)
			prevNode->pNext = newNode;
		else
			*pn = newNode;
	}
}

static void processNewString(const char * buffer, int len)
{
	char * wordStart = buffer;
	char * wordEnd = buffer;

	int totalMsgLen = 0;
	WordNode * node = NULL;
	char * insertPoint = NULL;

	clearAllNodes(&wordRoot);
	outLen = 0;
	if(outMsg)
		kfree(outMsg);

	// find the start
	while(true)
	{
		while((wordStart < buffer + len - 1) && !isLetterOrNum(*wordStart))
			wordStart++;
		if(wordStart >= buffer + len || !isLetterOrNum(*wordStart))//maybe end because of len
			break;
		wordEnd = wordStart;
		while((wordEnd < buffer + len - 1) && isLetterOrNum(*wordEnd))
			wordEnd++;
		if(isLetterOrNum(*wordEnd))// ti qian jie shu
			insertWordFromBuffer(&wordRoot, wordStart, wordEnd - wordStart + 1);
		else
			insertWordFromBuffer(&wordRoot, wordStart, wordEnd - wordStart);
		//printk(KERN_ALERT "Didi: %c", *(wordEnd - 1));
		wordStart = wordEnd + 1;
	}

	node = wordRoot;
	// Gen report

	while(node)
	{
		totalMsgLen += getRequiredChars(node);
		// printk(KERN_ALERT "MyCharDev : %s -> %d", node->word, node->count);
		node = node->pNext;
	}

	outLen = totalMsgLen;
	outMsg = (char *)kmalloc(outLen + 1, GFP_KERNEL);
	insertPoint = outMsg;

	node = wordRoot;
	while(node)
	{
		insertPoint += sprintf(insertPoint, node->word);
		insertPoint += sprintf(insertPoint, " ");
		insertPoint += sprintf(insertPoint, "%d", node->count);
		insertPoint += sprintf(insertPoint, "\n");
		node = node->pNext;
	}
	// printk(KERN_INFO "MyCharDev: %s", outMsg);
}


static struct file_operations fops =
{
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

static int __init mychardev_init(void)
{
	printk(KERN_INFO "MyCharDev: Initializing the MyCharDev Kernel Module\n");

	// Dynamically allocate a major number for this device by passing in '0'
	majorNum = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNum < 0)
	{
		printk(KERN_ALERT "MyCharDev: couldn't register a major number\n");
		return -1;
	}
	printk(KERN_ALERT "MyCharDev: Registered successfully with Major number : %d\n", majorNum);
	


	// Register the device class
   mycharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(mycharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNum, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(mycharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "MyCharDev: device class registered correctly\n");

   // Register the device driver
   mycharDevice = device_create(mycharClass, NULL, MKDEV(majorNum, 0), NULL, DEVICE_NAME);
   if (IS_ERR(mycharDevice))
   {               // Clean up if there is an error
      class_destroy(mycharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNum, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(mycharDevice);
   }
   printk(KERN_INFO "MyCharDev: device created correctly\n"); // Made it! device was initialized
   return 0;

	outMsg = (char *)kmalloc(1, GFP_KERNEL);
	outMsg[0] = 0;
	outLen = 1;
	return SUCCESS;
}
static void __exit mychardev_cleanup(void)
{
	device_destroy(mycharClass, MKDEV(majorNum, 0));     // remove the device
	class_unregister(mycharClass);                          // unregister the device class
	class_destroy(mycharClass);                             // remove the device class
	unregister_chrdev(majorNum, DEVICE_NAME);             // unregister the major number
	clearAllNodes(&wordRoot);
	outLen = 0;
	if(outMsg)
		kfree(outMsg);

	unregister_chrdev(majorNum, DEVICE_NAME);
	printk(KERN_INFO "MyCharDev: Unload successfully.\n");
}

static int device_open(struct inode * inodep, struct file * filep)
{
	timesOpen++;
	printk(KERN_INFO "MyCharDev: Device has been opened %d time(s)\n", timesOpen);
	return 0;
}

static ssize_t device_read(struct file * filep, char * buffer, size_t len, loff_t * ppos)
{
	int errCount = 0;

	if(*ppos != 0)
	{
		return 0;
	}

	errCount = copy_to_user(buffer, outMsg, outLen);

	if(errCount == 0)
	{
		printk(KERN_ALERT "MyCharDev: Sent %d characters to the user.\n", outLen);
		*ppos = outLen;
		return outLen;
	}
	else
	{
		printk(KERN_ALERT "MyCharDev: Failed to send %d characters to the user.\n", errCount);
		return -EFAULT;// Failed return a bad address message
	}
}
static ssize_t device_write(struct file * filep, const char * buffer, size_t len, loff_t * offset)
{
	//printk(KERN_INFO "Write len: %d", len);
	processNewString(buffer, len);
	return len;
}
static int device_release(struct inode * inodep, struct file * filep)
{
	printk(KERN_INFO "MyCharDev: Device successfully closed.\n");
	return 0;
}

module_init(mychardev_init);
module_exit(mychardev_cleanup);

