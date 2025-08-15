const mqtt = require('mqtt');
const TelegramBot = require('node-telegram-bot-api');
const dotenv = require('dotenv');

// Configuração do ambiente
dotenv.config();

// Configurações MQTT
const MQTT_CONFIG = {
  url: process.env.MQTT_URL,
  port: process.env.MQTT_PORT || 8883,
  username: process.env.MQTT_USER,
  password: process.env.MQTT_PASS,
  topics: {
    peso: 'balanca/peso',
    status: 'balanca/status'
    }
};

// Inicialização do Bot do Telegram
const bot = new TelegramBot(process.env.TELEGRAM_TOKEN, { 
  polling: true,
  request: {
    timeout: 20000,
    agent: null
  }
});

// Tratamento de erros do Telegram Bot
bot.on('polling_error', (error) => {
  console.error('Erro no Telegram Bot:', error.message);
  
  if (error.code === 401) {
    console.error('Token do Telegram inválido. Verifique o TELEGRAM_TOKEN no .env');
    process.exit(1);
  }
});

// Conexão MQTT
console.log('Conectando ao MQTT...');
const mqttClient = mqtt.connect(MQTT_CONFIG.url, {
  port: MQTT_CONFIG.port,
  username: MQTT_CONFIG.username,
  password: MQTT_CONFIG.password,
  rejectUnauthorized: false,
  protocolVersion: 5,
  reconnectPeriod: 0
});

// Função segura para enviar mensagens
async function enviarMensagem(chatId, text) {
  try {
    const cleanText = text.replace(/[_*[\]()~`>#+-=|{}.!]/g, '\\$&');
    await bot.sendMessage(chatId, cleanText, {
      parse_mode: 'MarkdownV2',
      disable_web_page_preview: true
    });
  } catch (error) {
    console.error('Erro ao enviar mensagem:', error.message);
    try {
      await bot.sendMessage(chatId, text); // Fallback sem formatação
    } catch (fallbackError) {
      console.error('Erro no fallback de mensagem:', fallbackError.message);
    }
  }
}

// Tratamento de mensagens MQTT
mqttClient.on('message', (topic, message) => {
  const msg = message.toString();
  console.log(`[MQTT] ${topic}: ${msg}`);
  
  const chatId = process.env.CHAT_ID;
  if (!chatId) {
    console.log('CHAT_ID não definido, mensagem não enviada');
    return;
  }

  const formattedMsg = topic.includes('status') 
    ? `🔔 Status: ${msg}`
    : `⚖️ Peso atual: *${msg} kg*`;
  
    enviarMensagem(chatId, formattedMsg);
});

// Comandos do bot
bot.onText(/\/start/, async (msg) => {
  try {
    const welcomeMsg = `*Bem-vindo ao Monitor de Balança* ⚖️\n\n` +
                      `Recebendo dados do sensor...\n` +
                      `Status importantes serão enviados aqui.`;
    await enviarMensagem(msg.chat.id, welcomeMsg);
  } catch (error) {
    console.error('Erro no comando /start:', error);
  }
});

bot.onText(/\/status/, (msg) => {
  mqttClient.publish('balanca/status', 'Solicitação de status', { qos: 1 });
});

// Conexão MQTT
mqttClient.on('connect', () => {
  console.log('Conectado ao broker MQTT');
  
  // Subscribe nos tópicos
  mqttClient.subscribe([MQTT_CONFIG.topics.peso, MQTT_CONFIG.topics.status], (err) => {
  if (err) {
    console.error('Erro ao subscrever tópicos:', err);
  } else {
    console.log('Subscrito nos tópicos: peso, status');
    
    if (process.env.CHAT_ID) {
      enviarMensagem(process.env.CHAT_ID, "✅ *Conectado ao servidor MQTT!*");
    }
  }
});
});

mqttClient.on('error', (err) => {
  console.error('Erro na conexão MQTT:', err.message);
  
  if (process.env.CHAT_ID) {
 enviarMensagem(process.env.CHAT_ID, `⚠️ *Erro MQTT:* ${err.message}`);
  }
});

// Tratamento de encerramento
process.on('SIGINT', () => {
  console.log('Encerrando aplicação...');
  mqttClient.end();
  process.exit();
});

console.log('Bot iniciado. Aguardando comandos...');