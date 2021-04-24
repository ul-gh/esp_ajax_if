<!-- ESP Ajax Lab Network and Updating Component
-->
<template>
  <div class="network_and_update">
    <form 
      @submit.prevent="submit_net_conf"
      action="/set_wifi_config"
      autocomplete="off"
    >
      <table>
        <caption class="info_text">
          <h2>
            WiFi / Network Settings
          </h2>
          <p class="align-left">
            You can set either access point mode or station mode.
          </p>
          <p class="align-left">
            For station mode, you can set a fixed IP or auto / DHCP. 
            Access point always has a static IP and runs a DHCP and DNS server 
            for which you can set a custom hostname.
          </p>
        </caption>
        <thead>
          <tr>
            <th colspan="2">Network Settings</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    :value="true"
                    checked
                    v-model="net_conf.ap_mode_active"
                    @click="set_ap_mode_defaults"
                >
                Access Point
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    :value="false"
                    v-model="net_conf.ap_mode_active"
                >
                Station
              </label>
              </span>
            </td>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    :value="false"
                    v-model="net_conf.sta_use_dhcp"
                >
                Fixed IP
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    :value="true"
                    checked
                    v-model="net_conf.sta_use_dhcp"
                >
                Auto / DHCP
              </label>
              </span>
            </td>
          </tr>
          <tr>
            <td>
              <span class="flex-centered-row">
              <label class="flex-stacked-calign">
                <input
                    type="checkbox"
                    v-model="net_conf.dns_active"
                >
                Enable DNS
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="checkbox"
                    v-model="net_conf.mdns_active"
                >
                Enable MDNS
              </label>
              </span>
            </td>
            <td>
              <label class="flex-stacked-calign">
                WiFi PSK / Password
                <input
                  type="password"
                  v-model="net_conf.psk"
                >
              </label>
            </td>
          </tr>
          <tr>
            <td>
              <label class="flex-stacked-calign">
                DNS / DHCP Hostname
                <input
                  type="text"
                  v-model="net_conf.hostname"
                >
              </label>
            </td>
            <td>
              <label class="flex-stacked-calign">
                WiFi Name / SSID
                <input
                  type="text"
                  v-model="net_conf.ssid"
                >
              </label>
            </td>
          </tr>
          <tr>
            <td>
              <label class="flex-stacked-calign">
                IP Address
                <input
                  type="text"
                  v-model="net_conf.ip4_addr"
                >
              </label>
            </td>
            <!-- Omitting the gateway IP configuration box - do we ever need to change the default address?
            <td>
              <label class="flex-stacked-calign">
                IP Address
                <input
                  type="text"
                  v-model="net_conf.ip4_gw"
                >
              </label>
            </td>
            -->
            <td>
              <label class="flex-stacked-calign">
                Subnet Mask
                <input
                  type="text"
                  v-model="net_conf.ip4_mask"
                >
              </label>
            </td>
          </tr>
          <tr>
            <td colspan="2">
              <input
                type="submit"
                class="button_large"
                value="Submit!"
              >
            </td>
          </tr>
        </tbody>
      </table>
    </form>

    <form action="/update" method="post" enctype="multipart/form-data">
      <table>
        <caption class="info_text">
          <h2>
            Over-The-Air Firmware Update
          </h2>
          <p class="align-left">
            1.) Select a local ".bin" firmware file
          </p>
          <p class="align-left">
            2.) Click the "Perform Update Now!" button to start the upload/update
          </p>
          <p>
            ATTENTION: There is no integrity checks and a wrong firmware file will
            render the hardware non-functional or show unwanted behaviour!
          </p>
        </caption>
        <thead>
          <tr>
            <th>OTA Firmware Upload</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><input type="file" name="update" /></td>
          </tr>
          <tr>
            <td>
              <input
                type="submit"
                class="button_large"
                value="Perform Update Now!"
              />
            </td>
          </tr>
        </tbody>
      </table>
    </form>
  </div>
</template>

<script>
import { do_json_request } from "@/api/async_requests_sse.js";

export default {
  name: "NetworkAndUpdate",
  components: {},
  data() {
    return {
      net_conf: {
        //// Network configuration (values are only defaults)
        ip4_addr: "192.168.4.1",
        ip4_gw: "192.168.4.1",
        ip4_mask: "255.255.0.0",
        hostname: "eal",
        ssid: "esp_ajax_lab",
        psk: "123FOO456",
        ap_mode_active: true,
        sta_use_dhcp: true,
        dns_active: true,
        mdns_active: false,
      },
    };
  },
  props: {
    state: Object,
    disabled: Boolean
  },
  methods: {
    async get_initial_state() {
      const response = await do_json_request("/get_wifi_config");
      this.update_net_conf(response);
    },
    async submit_net_conf(event) {
      if (this.disabled) {return;}
      const endpoint = event.target.action;
      const response = await do_json_request(endpoint, this.net_conf, 'POST');
      this.update_net_conf(response);
    },
    update_net_conf(response_obj) {
      console.log("Updating net_conf with: ", response_obj);
      Object.keys(this.net_conf).forEach(key => {
        if (response_obj.hasOwnProperty(key)) {
          this.net_conf[key] = response_obj[key];
        }
      });
    },
    // It is too easy to accidentially set a previously set infrastructure SSID
    // as our AP SSID, so the fields are pre-populated with defaults.
    set_ap_mode_defaults() {
      this.net_conf.ssid = 'esp_ajax_lab';
      this.net_conf.psk = '123FOO456';
    },
  },
  emits: ["action-triggered"],
  mounted() {
    this.get_initial_state();
    window.tab = this;
  },
};
</script>


<style scoped>
h2 {
  text-align: center;
}

/* caption {
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
} */

label {
  font-weight: bold;
}

input[type="text"],
input[type="password"] {
  width: 13rem;
}

.align-left {
  text-align: left;
  margin-left: 0.5rem;
}
</style>
