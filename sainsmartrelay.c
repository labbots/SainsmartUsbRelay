#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ftdi.h>
#include <getopt.h>
#include <ctype.h>

#include "sainsmartrelay.h"


static struct ftdi_context *ftdi;
static uint8 g_num_relays=MAX_NUM_RELAYS;

static void usage(char *myName)
{
    fprintf(stderr, "\nUsage:\n");
    fprintf(stderr, "  %s --on [1|2|3|4|all]\n", myName);
    fprintf(stderr, "  %s --off [1|2|3|4|all]\n", myName);
    fprintf(stderr, "  %s --status [1|2|3|4|all]\n", myName);
    fprintf(stderr, "  %s --findall\n", myName);
    fprintf(stderr, "  %s -h\n", myName);
}

static void help(char *myName)
{
    fprintf(stdout, "\nHelp:\n %s --on [1|2|3|4|all] | --off [1|2|3|4|all] | --status | --findall | [-h]\n", myName);
    fprintf(stdout, "  --help|-h  print this help.\n");
    fprintf(stdout, "  --on | -o [1|2|3|4|all]  switch specified relay output on.This argument also allows comma seperated relay numbers.\n");
    fprintf(stdout, "  --off | -f [1|2|3|4|all]  switch specified relay output off.This argument also allows comma seperated relay numbers.\n");
    fprintf(stdout, "  --status | -s [1|2|3|4|all] get the relay status.\n");
    fprintf(stdout, "  --findall | -a find all the FTDI device connected to the system.\n");
}

static void checkPermission()
{
    if(geteuid() != 0)
    {
        fprintf(stderr,"\nWarning:\n this program is currently not running with root priviledges !\n");
        fprintf(stderr,"Therefore it might not be able to access your relay cards communication port.\n");
        fprintf(stderr,"Consider invoking the program from the root account or use \"sudo ...\"\n");
    }
}



/**********************************************************
 * Function strsplit()
 *
 * Description: Split a given string based on delimiter
 *
 * Parameters: str (in) - String to be split
 *             delim(in)- Delimiter with which the string to be split.
 *             numtokens(out) - Size of the new response array
 *
 * Return:  array - response array
 *********************************************************/
char **strsplit(const char* str, const char* delim, size_t* numtokens)
{
    // copy the original string so that we don't overwrite parts of it
    // (don't do this if you don't need to keep the old line,
    // as this is less efficient)
    char *s = strdup(str);
    // these three variables are part of a very common idiom to
    // implement a dynamically-growing array
    size_t tokens_alloc = 1;
    size_t tokens_used = 0;
    char **tokens = calloc(tokens_alloc, sizeof(char*));
    char *token, *strtok_ctx;
    for (token = strtok_r(s, delim, &strtok_ctx);
            token != NULL;
            token = strtok_r(NULL, delim, &strtok_ctx))
    {
        // check if we need to allocate more space for tokens
        if (tokens_used == tokens_alloc)
        {
            tokens_alloc *= 2;
            tokens = realloc(tokens, tokens_alloc * sizeof(char*));
        }
        tokens[tokens_used++] = strdup(token);
    }
    // cleanup
    if (tokens_used == 0)
    {
        free(tokens);
        tokens = NULL;
    }
    else
    {
        tokens = realloc(tokens, tokens_used * sizeof(char*));
    }
    *numtokens = tokens_used;
    free(s);
    return tokens;
}


/**********************************************************
 * Function remove_duplicate()
 *
 * Description: Remove duplicate elements from input array
 *
 * Parameters: array (in) - source array
 *             length(in)- length of the source array
 *             numtokens(out) - Size of the new response array
 *
 * Return:  array - New array with deduped array content
 *********************************************************/
int *remove_duplicate(int array[],int length, size_t* numtokens)
{
    size_t tokens_alloc = 1;
    size_t tokens_used = 0;
    int *tokens = calloc(tokens_alloc, sizeof(int*));

    int *current , *end = array + length - 1;
    int flag = 0;
    for ( current = array + 1; array < end; array++, current = array + 1 )
    {
        flag = 0;
        while ( current <= end )
        {
            if ( *current == *array )
            {
                *current = *end--;

            }
            else
            {
                flag = 1;
                current++;
            }
        }
        if(flag == 1)
        {
            if (tokens_used == tokens_alloc)
            {
                tokens_alloc *= 1;
                tokens = realloc(tokens, tokens_alloc * sizeof(int*));
            }

            tokens[tokens_used++] = *array;
        }
    }
    if (tokens_used == tokens_alloc)
    {
        tokens_alloc *= 1;
        tokens = realloc(tokens, tokens_alloc * sizeof(int*));
    }

    tokens[tokens_used++] = *array;
    if (tokens_used == 0)
    {
        free(tokens);
        tokens = NULL;
    }
    else
    {
        tokens = realloc(tokens, tokens_used * sizeof(int*));
    }
    *numtokens = tokens_used;
    return tokens;
}

/**********************************************************
 * Function get_bits()
 *
 * Description: Calculate the relay status from bit information
 *
 * Parameters: n (in) - input bit information
 *             bitwanted(in)- status of relay required
 *
 * Return:  array of relay id and it's status
 *********************************************************/
int *get_bits(int n, int bitswanted)
{
    int *bits = malloc(sizeof(int) * bitswanted);

    int k;
    for(k=0; k<bitswanted; k++)
    {
        int mask =  1 << k;
        int masked_n = n & mask;
        int thebit = masked_n >> k;
        bits[k] = thebit;
    }

    return bits;
}

/**********************************************************
 * Function detect_relay_card_sainsmart_4_8chan()
 *
 * Description: Detect the Sainsmart USB relay card
 *
 * Parameters: portname (out) - pointer to a string where
 *                              the detected com port will
 *                              be stored
 *             num_relays(out)- pointer to number of relays
 *
 * Return:  0 - success
 *         -1 - fail, no relay card found
 *********************************************************/
int detect_relay_card_sainsmart_4_8chan(char* portname, uint8* num_relays)
{
    unsigned int chipid;

    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return -1;
    }

    /* Try to open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        ftdi_free(ftdi);
        return -1;
    }

    /* Set FTDI chip to bitbang mode */
    if (ftdi_set_bitmode(ftdi, 0xFF, BITMODE_BITBANG) < 0)
    {
        fprintf(stderr, "unable to set bitbang mode: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -1;
    }

    /* Check if this is an R type chip
    * Type 245RL = 5000
    */
    //printf("relay type:%d\n",ftdi->type);
    if (ftdi->type != 5000 && ftdi->type != TYPE_R )
    {
        fprintf(stderr, "unable to continue, not an R-type chip\n");
        ftdi_free(ftdi);
        return -1;
    }

    /* Read out FTDI Chip-ID of R type chips */
    ftdi_read_chipid(ftdi, &chipid);

    /* Return parameters */
    if (num_relays!=NULL) *num_relays = g_num_relays;
    sprintf(portname, "FTDI chipid %X", chipid);
    //printf("DBG: portname %s\n", portname);

    ftdi_usb_close(ftdi);
    return 0;
}


/**********************************************************
 * Function find_device()
 *
 * Description: Prints the list of device connected to the system
 *
 *
 * Return:  0 - success
 *         -1 - fail, no relay card found
 *********************************************************/
int find_device(void)
{
    int ret, i;
    struct ftdi_context *ftdi;
    struct ftdi_device_list *devlist, *curdev;
    char manufacturer[128], description[128];
    int retval = EXIT_SUCCESS;

    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    if ((ret = ftdi_usb_find_all(ftdi, &devlist, 0, 0)) < 0)
    {
        fprintf(stderr, "ftdi_usb_find_all failed: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
        retval =  EXIT_FAILURE;
        goto do_deinit;
    }

    printf("Number of FTDI devices found: %d\n", ret);

    i = 0;
    for (curdev = devlist; curdev != NULL; i++)
    {
        printf("Checking device: %d\n", i);
        if ((ret = ftdi_usb_get_strings(ftdi, curdev->dev, manufacturer, 128, description, 128, NULL, 0)) < 0)
        {
            fprintf(stderr, "ftdi_usb_get_strings failed: %d (%s)\n", ret, ftdi_get_error_string(ftdi));
            retval = EXIT_FAILURE;
            goto done;
        }
        printf("Manufacturer: %s, Description: %s\n", manufacturer, description);
        curdev = curdev->next;
    }
done:
    ftdi_list_free(&devlist);
do_deinit:
    ftdi_free(ftdi);
    return retval;
}

/**********************************************************
 * Function get_relay_sainsmart_4_8chan()
 *
 * Description: Get the current relay state
 *
 * Parameters: relay (in)        - relay number
 *             relay_state (out) - current relay state
 *
 * Return:    0 - success
 *          < 0 - fail
 *********************************************************/
int get_relay_sainsmart_4_8chan(uint8 relay, relay_state_t* relay_state)
{
    unsigned char buf[1];

    if (relay<FIRST_RELAY || relay>(FIRST_RELAY+g_num_relays-1))
    {
        fprintf(stderr, "ERROR: Relay number out of range\n");
        return -1;
    }

    /* Open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -2;
    }

    /* Get relay state from the card */
    if (ftdi_read_pins(ftdi, &buf[0]) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -3;
    }
    //printf("DBG: Read GPIO bits %02X\n", buf[0]);
    int *bits = get_bits(buf[0], g_num_relays);

    relay = relay-1;
    *relay_state = (bits[relay] > 0) ? ON : OFF;

    ftdi_usb_close(ftdi);
    return 0;
}

/**********************************************************
 * Function get_relay_sainsmart_4_8chan_all()
 *
 * Description: Get all the relay state
 *
 * Parameters: relay_states (out) - current state of all relays
 *
 * Return:    0 - success
 *          < 0 - fail
 *********************************************************/
int get_relay_sainsmart_4_8chan_all(int *relay_states)
{
    unsigned char buf[1];

    /* Open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -2;
    }

    /* Get relay state from the card */
    if (ftdi_read_pins(ftdi, &buf[0]) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -3;
    }
    //printf("DBG: Read GPIO bits %02X\n", buf[0]);
    int *bits = get_bits(buf[0], g_num_relays);

    int j;
    for(j=0; j<g_num_relays; j++)
    {
        relay_states[j]= bits[j];
    }
    ftdi_usb_close(ftdi);
    return 0;
}

/**********************************************************
 * Function get_relay_sainsmart_4_8chan_raw()
 *
 * Description: Get the current relay state
 *
 * Parameters: relay_data (out)  - Raw hex data from relay
 *
 * Return:    0 - success
 *          < 0 - fail
 *********************************************************/
int get_relay_sainsmart_4_8chan_raw(uint8 *relay_data)
{
    unsigned char buf[1];

    /* Open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -2;
    }

    /* Get relay state from the card */
    if (ftdi_read_pins(ftdi, &buf[0]) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -3;
    }
    *relay_data = buf[0];
    //printf("DBG: Read GPIO bits %02X\n", buf[0]);

    ftdi_usb_close(ftdi);
    return 0;
}
/**********************************************************
 * Function set_relay_sainsmart_4_8chan()
 *
 * Description: Set new relay state
 *
 * Parameters: relay (in)        - relay number
 *             relay_state (in)  - current relay state
 *
 * Return:    0 - success
 *          < 0 - fail
 *********************************************************/
int set_relay_sainsmart_4_8chan(uint8 relay, relay_state_t relay_state)
{
    unsigned char buf[1];

    if (relay<FIRST_RELAY || relay>(FIRST_RELAY+g_num_relays-1))
    {
        fprintf(stderr, "ERROR: Relay number out of range\n");
        return -1;
    }

    /* Open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -2;
    }

    /* Get relay state from the card */
    if (ftdi_read_pins(ftdi, buf) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -3;
    }

    /* Set the new relay state bit */
    relay = relay-1;
    if (relay_state == OFF)
    {
        /* Clear the relay bit in mask */
        buf[0] = buf[0] & ~(0x01<<relay);
    }
    else
    {
        /* Set the relay bit in mask */
        buf[0] = buf[0] | (0x01<<relay);
    }

    //printf("DBG: Writing GPIO bits %02X\n", buf[0]);

    /* Set relay on the card */
    if (ftdi_write_data(ftdi, buf, 1) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -4;
    }

    ftdi_usb_close(ftdi);
    return 0;
}

/**********************************************************
 * Function set_relay_sainsmart_4_8chan_all()
 *
 * Description: Set new relay state for all channel
 *
 * Parameters: relay_state (in)  - new relay state for all relays
 *
 * Return:    0 - success
 *          < 0 - fail
 *********************************************************/
int set_relay_sainsmart_4_8chan_all(relay_state_t relay_state)
{
    unsigned char buf[1];
    int i;

    /* Open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -2;
    }

    /* Get relay state from the card */
    if (ftdi_read_pins(ftdi, buf) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -3;
    }

    /* Set the new relay state bit */
    if (relay_state == OFF)
    {
        /* Clear all the relay state */
        //buf[0] = 0x0;
        for(i=1; i<= g_num_relays; i++)
        {
            buf[0] = buf[0] & ~(0x01<<(i-1));

        }
    }
    else
    {
        /* Set all the relay state */
        //buf[0] = 0xFF;
        for(i=1; i<= g_num_relays; i++)
        {
            buf[0] = buf[0] | (0x01<<(i-1));

        }
    }

    //printf("DBG: Writing GPIO bits %02X\n", buf[0]);

    /* Set relay on the card */
    if (ftdi_write_data(ftdi, buf, 1) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -4;
    }

    ftdi_usb_close(ftdi);
    return 0;
}

/**********************************************************
 * Function set_relay_sainsmart_4_8chan_write()
 *
 * Description: Set new relay state
 *
 * Parameters: relay_data (in)        - relay data
 *
 * Return:    0 - success
 *          < 0 - fail
 *********************************************************/
int set_relay_sainsmart_4_8chan_write(uint8 relay_data)
{
    unsigned char buf[1];

    /* Open FTDI USB device */
    if ((ftdi_usb_open(ftdi, VENDOR_ID, DEVICE_ID)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: (%s)\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return -2;
    }

    /* Set the new relay state bit */
    buf[0] = relay_data;

    //printf("DBG: Writing GPIO bits %02X\n", buf[0]);

    /* Set relay on the card */
    if (ftdi_write_data(ftdi, buf, 1) < 0)
    {
        fprintf(stderr,"read failed for 0x%x, error %s\n",buf[0], ftdi_get_error_string(ftdi));
        return -4;
    }

    ftdi_usb_close(ftdi);
    return 0;
}

int main(int argc, char *argv[])
{
    relay_state_t rstate;
    char com_port[MAX_COM_PORT_NAME_LEN];
    uint8 num_relays=FIRST_RELAY;
    int opt;
    int long_index = 0;
    int all_check_flag = 0;
    char *op_relay_on;
    char *op_relay_off;
    int opOn = -1,opOff = -1;

    static struct option long_options[] =
    {
        {"help",      no_argument,       0,  'h' },
        {"findall", no_argument,       0,  'a' },
        {"on",   required_argument, 0,  'o' },
        {"off",   required_argument, 0,  'f' },
        {"status",   required_argument, 0,  's' },
        {0,           0,                 0,  0   }
    };
    if(argc < 2)
    {
        fprintf(stderr, "too few arguments!\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt_long(argc, argv,":has:o:f:",
                              long_options, &long_index )) != -1)
    {

        switch (opt)
        {
        case 'a' :
            find_device();
            exit(EXIT_SUCCESS);
            break;
        case 'o' :
            if (detect_relay_card_sainsmart_4_8chan(com_port, &num_relays) == -1)
            {
                fprintf(stderr,"No compatible device detected.\n");
                checkPermission();
                exit(EXIT_FAILURE);
            }

            if (strcasecmp(optarg, "all") == 0)
            {
                if(all_check_flag == 1)
                {
                    fprintf(stderr, "invalid arguments. 'all' value is already set to --off argument\n");
                    exit(EXIT_FAILURE);
                }
                all_check_flag = 1;
                opOn = ID_ON_ALL;
                break;
            }
            else if(strchr(optarg, ',') == 0)
            {
                op_relay_on = strdup(optarg);
                opOn = ID_ON;
                break;
            }
            else
            {
                op_relay_on = strdup(optarg);
                opOn = ID_ON_MULTIPLE;
                break;
            }
            break;
        case 'f' :
            if (detect_relay_card_sainsmart_4_8chan(com_port, &num_relays) == -1)
            {
                fprintf(stderr,"No compatible device detected.\n");
                checkPermission();
                exit(EXIT_FAILURE);
            }

            if (strcasecmp(optarg, "all") == 0)
            {
                if(all_check_flag == 1)
                {
                    fprintf(stderr, "invalid arguments. 'all' value is already set to --off argument\n");
                    exit(EXIT_FAILURE);
                }
                all_check_flag = 1;
                opOff = ID_OFF_ALL;
                break;
            }
            else if(strchr(optarg, ',') == 0)
            {
                op_relay_off = strdup(optarg);
                opOff = ID_OFF;
                break;
            }
            else
            {
                op_relay_off = strdup(optarg);
                opOff = ID_OFF_MULTIPLE;
                break;
            }
            break;
        case 's' :
            if (detect_relay_card_sainsmart_4_8chan(com_port, &num_relays) == -1)
            {
                fprintf(stderr,"No compatible device detected.\n");
                checkPermission();
                exit(EXIT_FAILURE);
            }
            if (strcasecmp(optarg, "all") == 0)
            {
                int relay_states[g_num_relays-1];
                if (get_relay_sainsmart_4_8chan_all(relay_states) == 0)
                {
                    int j;
                    for(j=0; j<g_num_relays; j++)
                    {
                        fprintf(stdout, "%d: %s\n", j+1,(relay_states[j] > 0) ? "ON" : "OFF");
                    }
                    exit(EXIT_SUCCESS);
                }
            }
            else if(isdigit(optarg[0]))
            {
                if (get_relay_sainsmart_4_8chan(atoi(optarg), &rstate) == 0)
                {
                    fprintf(stdout, "%d: %s\n", atoi(optarg),(rstate==ON) ? "ON" : "OFF");
                    exit(EXIT_SUCCESS);
                }
            }
            else
            {
                fprintf(stderr, "invalid value is set to --status argument\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'h' :
            help(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case ':':
            fprintf(stderr, "%s: option `-%c' requires an argument\n",argv[0], optopt);
            exit(EXIT_FAILURE);
            break;
        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }


    char **op_relay_list = {0};
    size_t numtokens = 0;
    int i = 0;
    size_t numtok = 0;
    int *relays;

    /*
    * Get the current status of the relay
    */
    uint8 relay_data;
    if (get_relay_sainsmart_4_8chan_raw(&relay_data) != 0)
    {
        fprintf(stderr, "Error reading from the relay");
    }

    /*
    * Process all the multiple relay ON state operation
    */

    if(opOn == ID_ON_ALL)
    {
        for(i=1; i<= g_num_relays; i++)
        {
            relay_data = relay_data | (0x01<<(i-1));

        }
    }
    else if(opOn == ID_ON)
    {

        relay_data = relay_data | (0x01<<(atoi(op_relay_on)-1));
    }
    else if(opOn == ID_ON_MULTIPLE)
    {

        op_relay_list = strsplit(op_relay_on, ", \t\n", &numtokens);
        int relay_list[numtokens];
        for (i = 0; i < numtokens; i++)
        {
            relay_list[i] = atoi(strdup(op_relay_list[i]));
            free(op_relay_list[i]);
        }
        relays = remove_duplicate(relay_list,numtokens,&numtok);

        for(i=0; i< numtok; i++)
        {
            if(relays[i] != 0)
            {
                relay_data = relay_data | (0x01<<(relays[i]-1));
            }

        }


    }

    /*
    * Process all the multiple relay OFF state operation
    */
    if(opOff == ID_OFF_ALL)
    {
        for(i=1; i<= g_num_relays; i++)
        {
            relay_data = relay_data & ~(0x01<<(i-1));

        }
    }
    else if(opOff == ID_OFF)
    {

        relay_data = relay_data & ~(0x01<<(atoi(op_relay_off)-1));
    }
    else if(opOff == ID_OFF_MULTIPLE)
    {
        op_relay_list = strsplit(op_relay_off, ", \t\n", &numtokens);
        int relay_list[numtokens];
        for (i = 0; i < numtokens; i++)
        {
            relay_list[i] = atoi(strdup(op_relay_list[i]));
            free(op_relay_list[i]);
        }
        relays = remove_duplicate(relay_list,numtokens,&numtok);

        for(i=0; i< numtok; i++)
        {
            if(relays[i] != 0)
            {
                relay_data = relay_data & ~(0x01<<(relays[i]-1));
            }
        }
    }

    /*
    * Write the final state data into the relay
    */
    if(opOn != -1 || opOff != -1)
    {
        if (set_relay_sainsmart_4_8chan_write(relay_data) == 0)
        {
            int relay_states[g_num_relays-1];
            if (get_relay_sainsmart_4_8chan_all(relay_states) == 0)
            {
                int j;
                for(j=0; j<g_num_relays; j++)
                {
                    fprintf(stdout, "%d: %s\n", j+1,(relay_states[j] > 0) ? "ON" : "OFF");
                }
                exit(EXIT_SUCCESS);
            }
        }
        else
        {
            fprintf(stderr, "Error writing data to the relay.\n");
            exit(EXIT_FAILURE);
        }


    }
    exit(EXIT_SUCCESS);
}

