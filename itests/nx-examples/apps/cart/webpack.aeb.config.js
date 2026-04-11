const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');

const rootDir = path.resolve(__dirname, '../..');

module.exports = {
  mode: 'production',
  entry: path.resolve(__dirname, 'src/main.tsx'),
  output: {
    path: path.resolve(rootDir, 'dist/apps/cart'),
    filename: '[name].[contenthash].js',
    clean: true,
  },
  resolve: {
    extensions: ['.tsx', '.ts', '.js', '.jsx'],
    alias: (() => {
      // Build aliases from tsconfig.base.json paths
      const tsconfig = require('../../tsconfig.base.json');
      const paths = tsconfig.compilerOptions.paths || {};
      const aliases = {};
      for (const [alias, targets] of Object.entries(paths)) {
        const cleanAlias = alias.replace('/*', '');
        const target = path.resolve(rootDir, targets[0].replace('/*', '').replace('/src/index.ts', '/src'));
        aliases[cleanAlias] = target;
      }
      return aliases;
    })(),
  },
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: {
          loader: 'ts-loader',
          options: {
            configFile: path.resolve(__dirname, 'tsconfig.app.json'),
            transpileOnly: true,
          },
        },
        exclude: /node_modules/,
      },
      {
        test: /\.css$/,
        use: ['style-loader', 'css-loader'],
      },
      {
        test: /\.scss$/,
        use: ['style-loader', 'css-loader', 'sass-loader'],
      },
      {
        test: /\.(png|jpe?g|gif|svg|ico)$/,
        type: 'asset/resource',
      },
    ],
  },
  plugins: [
    new HtmlWebpackPlugin({
      template: path.resolve(__dirname, 'src/index.html'),
    }),
  ],
};
