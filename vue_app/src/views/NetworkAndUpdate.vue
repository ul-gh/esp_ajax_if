<!-- ESP Ajax Lab Network and Updating Component
-->
<template>
  <div class="network_and_update">
    <form 
      @submit.prevent="submit_config"
      action="/set_wifi_config"
      method="POST">
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
                    name="ap_mode_active"
                    :value="true"
                    checked
                    v-model="ap_mode_active"
                >
                Access Point
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="ap_mode_active"
                    :value="false"
                    v-model="ap_mode_active"
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
                    name="sta_use_dhcp"
                    :value="false"
                    v-model="sta_mode_use_dhcp"
                >
                Fixed IP
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="radio"
                    name="sta_use_dhcp"
                    :value="true"
                    checked
                    v-model="sta_mode_use_dhcp"
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
                    name="dns_active"
                    v-model="dns_active"
                >
                Enable DNS
              </label>
              <label class="flex-stacked-calign">
                <input
                    type="checkbox"
                    name="mdns_active"
                    v-model="mdns_active"
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
                  name="psk"
                  v-model="psk"
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
                  name="hostname"
                  v-model="hostname"
                >
              </label>
            </td>
            <td>
              <label class="flex-stacked-calign">
                WiFi Name / SSID
                <input
                  type="text"
                  name="ssid"
                  v-model="ssid"
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
                  name="ip4_addr"
                  v-model="ip4_addr"
                >
              </label>
            </td>
            <!-- Omitting the gateway IP configuration box - do we ever need to change the default address?
            <td>
              <label class="flex-stacked-calign">
                IP Address
                <input
                  type="text"
                  name="ip4_gw"
                  v-model="ip4_gw"
                >
              </label>
            </td>
            -->
            <td>
              <label class="flex-stacked-calign">
                Subnet Mask
                <input
                  type="text"
                  name="ip4_mask"
                  v-model="ip4_mask"
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
export default {
  name: "NetworkAndUpdate",
  components: {},
  data() {
    return {
      //// Network configuration (values are only defaults)
      ip4_addr: "192.168.4.1",
      ip4_gw: "192.168.4.1",
      ip4_mask: "255.255.0.0",
      hostname: "eal",
      ssid: "esp_ajax_lab",
      psk: "123FOO456",
      ap_mode_active: true,
      sta_mode_use_dhcp: true,
      dns_active: true,
      mdns_active: false,
    };
  },
  props: {
    state: Object,
    disabled: Boolean
  },
  methods: {
    async get_initial_state() {
      const data_obj = await this.do_json_request("/get_wifi_config");
      this.update_data(data_obj);
    },
    async submit_config(event) {
      const form = event.target;
      if (form.hasAttribute("disabled")) {return;}
      // Form data as JSON
      //FormData keeps boolean attributes as string values, but the server
      // needs boolean. So we send the vue data object instead.
      //const form_data = new FormData(form);
      let request_obj = {};
      //this.$data.forEach((value, key) => request_obj[key] = value);
      for (let key in this.$data) {
        request_obj[key] = this.$data[key];
      }
      const data_obj = await this.do_json_request(form.action, request_obj, 'POST');
      this.update_data(data_obj);
    },
    async do_json_request(uri, request_obj=undefined, method='GET') {
      const json_str = JSON.stringify(request_obj);
      let request_body = undefined;
      if (method === 'POST') {
        request_body = json_str;
      } else if (method === 'GET') {
        if (json_str) {
          uri += `?${encodeURIComponent(json_str)}`;
        }
      }
      // Prepare fetch request
      const fetch_options = {
        method: method,
        //mode: 'cors', // no-cors, *cors, same-origin
        cache: 'no-cache',
        //credentials: 'same-origin', // include, *same-origin, omit
        headers: {
          'Content-Type': 'application/json'
          // 'Content-Type': 'application/x-www-form-urlencoded',
        },
        redirect: 'error', // manual, *follow, error
        // no-referrer, *no-referrer-when-downgrade, origin, origin-when-cross-origin,
        // same-origin, strict-origin, strict-origin-when-cross-origin, unsafe-url
        referrerPolicy: 'no-referrer',
        // body data type must match "Content-Type" header
        body: request_body,
      };

      const response = await fetch(uri, fetch_options);
      if (!response.ok) {
        alert("Server error, bad request or bad configuration...");
        return {};
      } else {
        const data_obj = await response.json();
        return data_obj;
      }
    },
    update_data(data_obj) {
      console.log("Updating data with: ", data_obj);
      for (let key in this.$data) {
        if (data_obj.hasOwnProperty(key)) {
          this.$data[key] = data_obj[key];
        }
      }
    }
  },
  emits: ["action-triggered"],
  mounted() {
    this.get_initial_state();
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
