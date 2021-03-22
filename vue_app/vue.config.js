const CompressionPlugin = require('compression-webpack-plugin');
const esp_ajax_if_compression_config = {
    filename: '[path].gz[query]',
    algorithm: 'gzip',
    include: /\.(js|css|html|svg|json)(\?.*)?$/i,
    minRatio: 0.8,
};

module.exports = {
    publicPath: "/static/",
    filenameHashing: false,
    chainWebpack(config) {       
        // Use GZIP compression, also in development mode
        config
            .plugin('CompressionPlugin')
            .use(CompressionPlugin)
            .init(Plugin => new Plugin(esp_ajax_if_compression_config));
    },
    productionSourceMap: process.env.NODE_ENV != 'production',
};
