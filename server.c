#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "mongoose.h"

//#define _DEBUG_LEVEL MG_LL_DEBUG
#define _DEBUG_LEVEL MG_LL_INFO

struct ws_data {
  int player_no;
  struct libevdev_uinput *uidev;
  struct libevdev *dev;
};

struct ws_list {
  struct ws_data data;
  struct ws_list *prev, *next;
};

static pthread_mutex_t ws_list_mx;

static struct ws_list ws_list_head = {
  {0, NULL, NULL},
  NULL,
  NULL
};

static struct ws_list *new_player(void)
{
  pthread_mutex_lock(&ws_list_mx);

  struct ws_list *ptr = &ws_list_head;
  while(ptr->next)
  {
    int diff = ptr->next->data.player_no - ptr->data.player_no;
    if(diff > 1)
    {
      // insert here
      break;
    }
    ptr = ptr->next;
  }
  // create after `ptr`
  struct ws_list *new_item = (struct ws_list*)malloc(sizeof(struct ws_list));
  if(!new_item)
  {
    pthread_mutex_unlock(&ws_list_mx);
    return NULL;
  }

  new_item->data.player_no = ptr->data.player_no + 1;
  new_item->prev = ptr;
  new_item->next = ptr->next;
  if(ptr->next)
    ptr->next->prev = new_item;
  ptr->next = new_item;

  pthread_mutex_unlock(&ws_list_mx);
  return new_item;
}

static void del_player(struct ws_list *item)
{
  if(item == &ws_list_head)
    return;

  pthread_mutex_lock(&ws_list_mx);

  item->prev->next = item->next;
  if(item->next)
    item->next->prev = item->prev;

  free(item);

  pthread_mutex_unlock(&ws_list_mx);
}

int setup_uinput(struct ws_data *ws_data)
{
  int err;
  struct libevdev *dev;
  struct libevdev_uinput *uidev;

  ws_data->uidev = NULL;
  ws_data->dev = NULL;

  dev = libevdev_new();
  if(!dev)
  {
    MG_ERROR(("failed to create dev device"));
    return 1;
  }
  libevdev_set_name(dev, "MobileGamePad");
  libevdev_set_id_bustype(dev, BUS_USB);
  libevdev_set_id_vendor(dev, 0x5);
  libevdev_set_id_product(dev, 0x5);
  libevdev_set_id_version(dev, 1);

  err = 0;

  err += libevdev_enable_event_type(dev, EV_KEY);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_A, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_B, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_X, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_Y, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_TL, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_TR, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_TL2, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_TR2, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_START, NULL);
  err += libevdev_enable_event_code(dev, EV_KEY, BTN_SELECT, NULL);

  struct input_absinfo
    abs_x = {
      127,
      0,
      255,
      0,
      15,
      16
    },
    abs_y = {
      127,
      0,
      255,
      0,
      15,
      16
    }
  ;
  err += libevdev_enable_event_type(dev, EV_ABS);
  err += libevdev_enable_event_code(dev, EV_ABS, ABS_X, &abs_x);
  err += libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &abs_y);

  if(err)  /* < 0 */
  {
    MG_ERROR(("failed to setup libevdev uinput"));
    libevdev_free(dev);
    return 1;
  }

  err = libevdev_uinput_create_from_device(
    dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
  if(err != 0)
  {
    MG_ERROR(("failed to open uinput"));
    libevdev_free(dev);
    return 1;
  }

  ws_data->uidev = uidev;
  ws_data->dev = dev;

  return 0;
}

static void http_handler(struct mg_connection *c, int ev, void *ev_data, void *user_data)
{
  if(ev == MG_EV_HTTP_MSG)
  {
    struct mg_http_message *hm = (struct mg_http_message*)ev_data;

    if(mg_http_match_uri(hm, "/websocket"))
    {
      mg_ws_upgrade(c, hm, NULL);
    }
    else
    {
      struct mg_http_serve_opts opts = {.root_dir = "./html"};
      mg_http_serve_dir(c, hm, &opts);
    }
  }
  else if(ev == MG_EV_WS_OPEN)
  {
    // setup libev and send the controller number to the client
    struct ws_list *ws_player = new_player();
    if(!ws_player)
    {
      MG_ERROR(("memory allocation failed"));
      // close connection
      c->is_draining = 1;
      return;
    }
    c->fn_data = ws_player;
    MG_INFO(("+ controller %d connected", ws_player->data.player_no));

    if(setup_uinput(&ws_player->data))
    {
      c->is_draining = 1;
      return;
    }

    // send back the controller number
    mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"player_no\": %d}", ws_player->data.player_no);
  }
  else if(ev == MG_EV_WS_MSG)
  {
    struct mg_ws_message *wm = (struct mg_ws_message*)ev_data;
    struct ws_list *player = (struct ws_list*)c->fn_data;

    long ev_type = mg_json_get_long(wm->data, "$.type", -1);
    long ev_code = mg_json_get_long(wm->data, "$.code", -1);
    long ev_value = mg_json_get_long(wm->data, "$.value", -1);

    if(ev_type < 0 || ev_code < 0 || ev_value < 0)
    {
      MG_ERROR(("missing fields in websocket message: type:%ld code:%ld value:%ld", ev_type, ev_code, ev_value));
      return;
    }

    if(libevdev_uinput_write_event(player->data.uidev, ev_type, ev_code, ev_value))
    {
      MG_ERROR(("failed to write event to evdev"));
      return;
    }
    if(libevdev_uinput_write_event(player->data.uidev, EV_SYN, SYN_REPORT, 0))
    {
      MG_ERROR(("failed to write syn to evdev"));
      return;
    }
  }
  else if(ev == MG_EV_CLOSE && c->is_websocket && c->fn_data != NULL)
  {
    struct ws_list *player = (struct ws_list*)c->fn_data;
    MG_INFO(("- controller %d disconnected", player->data.player_no));
    if(player->data.uidev)
      libevdev_uinput_destroy(player->data.uidev);
    if(player->data.dev)
      libevdev_free(player->data.dev);
    del_player(player);
  }
}

int main(int argc, char**argv)
{
  if(pthread_mutex_init(&ws_list_mx, NULL))
  {
    fprintf(stderr, "failed to init mutex\n");
    return 1;
  }

  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  mg_log_set(_DEBUG_LEVEL);
  if(mg_http_listen(&mgr, "http://0.0.0.0:8888", http_handler, NULL) == NULL)
  {
    fprintf(stderr, "failed to setup http listener\n");
    return 1;
  }

  for(;;)
    mg_mgr_poll(&mgr, 1000);


  return 0;
}
